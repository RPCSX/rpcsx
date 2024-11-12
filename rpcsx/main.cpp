#include "audio/AlsaDevice.hpp"
#include "backtrace.hpp"
#include "gpu/DeviceCtl.hpp"
#include "io-device.hpp"
#include "io-devices.hpp"
#include "iodev/mbus.hpp"
#include "iodev/mbus_av.hpp"
#include "ipmi.hpp"
#include "linker.hpp"
#include "ops.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/Config.hpp"
#include "rx/mem.hpp"
#include "rx/watchdog.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include "xbyak/xbyak.h"
#include <orbis/utils/Rc.hpp>
#include <print>
#include <rx/Version.hpp>
#include <rx/align.hpp>
#include <rx/hexdump.hpp>

#include <elf.h>
#include <linux/prctl.h>
#include <orbis/KernelContext.hpp>
#include <orbis/module.hpp>
#include <orbis/module/Module.hpp>
#include <orbis/sys/sysentry.hpp>
#include <orbis/sys/sysproto.hpp>
#include <orbis/thread/Process.hpp>
#include <orbis/thread/ProcessOps.hpp>
#include <orbis/thread/Thread.hpp>

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <ucontext.h>

#include <atomic>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <print>

static int g_gpuPid;
extern bool allowMonoDebug;

__attribute__((no_stack_protector)) static void
handle_signal(int sig, siginfo_t *info, void *ucontext) {
  if (auto hostFs = _readgsbase_u64()) {
    _writefsbase_u64(hostFs);
  }

  auto signalAddress = reinterpret_cast<std::uintptr_t>(info->si_addr);

  if (orbis::g_currentThread != nullptr &&
      orbis::g_currentThread->tproc->vmId >= 0 && sig == SIGSEGV &&
      signalAddress >= orbis::kMinAddress &&
      signalAddress < orbis::kMaxAddress) {
    auto vmid = orbis::g_currentThread->tproc->vmId;
    auto ctx = reinterpret_cast<ucontext_t *>(ucontext);
    bool isWrite = (ctx->uc_mcontext.gregs[REG_ERR] & 0x2) != 0;
    auto origVmProt = vm::getPageProtection(signalAddress);
    int prot = 0;
    auto page = signalAddress / rx::mem::pageSize;

    if (origVmProt & vm::kMapProtCpuRead) {
      prot |= PROT_READ;
    }
    if (origVmProt & vm::kMapProtCpuWrite) {
      prot |= PROT_WRITE;
    }
    if (origVmProt & vm::kMapProtCpuExec) {
      prot |= PROT_EXEC;
    }

    auto gpuDevice = amdgpu::DeviceCtl{orbis::g_context.gpuDevice};

    if (gpuDevice && (prot & (isWrite ? PROT_WRITE : PROT_READ)) != 0) {
      auto &gpuContext = gpuDevice.getContext();
      while (true) {
        auto flags =
            gpuContext.cachePages[vmid][page].load(std::memory_order::relaxed);

        if ((flags & amdgpu::kPageReadWriteLock) != 0) {
          if ((flags & amdgpu::kPageLazyLock) != 0) {
            if (std::uint32_t gpuCommand = 0;
                !gpuContext.gpuCacheCommand[vmid].compare_exchange_weak(
                    gpuCommand, page)) {
              continue;
            }

            gpuContext.gpuCacheCommandIdle.fetch_add(
                1, std::memory_order::release);
            gpuContext.gpuCacheCommandIdle.notify_all();

            while (!gpuContext.cachePages[vmid][page].compare_exchange_weak(
                flags, flags & ~amdgpu::kPageLazyLock,
                std::memory_order::relaxed)) {
            }
          }
          continue;
        }

        if ((flags & amdgpu::kPageWriteWatch) == 0) {
          break;
        }

        if (!isWrite) {
          prot &= ~PROT_WRITE;
          break;
        }

        if (gpuContext.cachePages[vmid][page].compare_exchange_weak(
                flags, amdgpu::kPageInvalidated, std::memory_order::relaxed)) {
          break;
        }
      }

      if (::mprotect((void *)(page * rx::mem::pageSize), rx::mem::pageSize,
                     prot)) {
        std::perror("cache reprotection error");
        std::abort();
      }

      _writefsbase_u64(orbis::g_currentThread->fsBase);
      return;
    }

    std::fprintf(stderr, "SIGSEGV, address %lx, access %s, prot %s\n",
                 signalAddress, isWrite ? "write" : "read",
                 vm::mapProtToString(origVmProt).c_str());
  }

  if (orbis::g_currentThread != nullptr) {
    orbis::g_currentThread->tproc->exitStatus = sig;
    orbis::g_currentThread->tproc->event.emit(orbis::kEvFiltProc,
                                              orbis::kNoteExit, sig);
  }

  if (g_gpuPid > 0) {
    // stop gpu thread
    // ::kill(g_gpuPid, SIGINT);
  }

  allowMonoDebug = true;
  if (sig != SIGINT) {
    char buf[128] = "";
    int len = snprintf(buf, sizeof(buf), " [%s] %u: Signal %u, address=%p\n",
                       orbis::g_currentThread ? "guest" : "host",
                       orbis::g_currentThread ? orbis::g_currentThread->tid
                                              : ::gettid(),
                       sig, info->si_addr);
    write(2, buf, len);

    if (std::size_t printed =
            rx::printAddressLocation(buf, sizeof(buf), orbis::g_currentThread,
                                     (std::uint64_t)info->si_addr)) {
      printed += std::snprintf(buf + printed, sizeof(buf) - printed, "\n");
      write(2, buf, printed);
    }

    if (orbis::g_currentThread) {
      rx::printStackTrace(reinterpret_cast<ucontext_t *>(ucontext),
                          orbis::g_currentThread, 2);
    } else {
      rx::printStackTrace(reinterpret_cast<ucontext_t *>(ucontext), 2);
    }
  }

  struct sigaction act{};
  sigset_t mask;
  sigemptyset(&mask);

  act.sa_handler = SIG_DFL;
  act.sa_flags = SA_SIGINFO | SA_ONSTACK;
  act.sa_mask = mask;

  if (sigaction(sig, &act, NULL)) {
    perror("Error sigaction:");
    std::exit(-1);
  }

  if (sig == SIGINT) {
    std::raise(SIGINT);
  }
}

void setupSigHandlers() {
  rx::thread::setupSignalStack();

  struct sigaction act{};
  act.sa_sigaction = handle_signal;
  act.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;

  if (sigaction(SIGSYS, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }

  if (sigaction(SIGILL, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }

  if (sigaction(SIGSEGV, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }

  if (sigaction(SIGBUS, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }

  if (sigaction(SIGABRT, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }

  if (sigaction(SIGFPE, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }
}

struct StackWriter {
  std::uint64_t address;

  template <typename T> std::uint64_t push(T value) {
    address -= sizeof(value);
    address &= ~(alignof(T) - 1);
    *reinterpret_cast<T *>(address) = value;
    return address;
  }

  void align(std::uint64_t alignment) { address &= ~(alignment - 1); }

  std::uint64_t pushString(const char *value) {
    auto len = std::strlen(value);
    address -= len + 1;
    std::memcpy(reinterpret_cast<void *>(address), value, len + 1);
    return address;
  }

  std::uint64_t alloc(std::uint64_t size, std::uint64_t alignment) {
    address -= size;
    address &= ~(alignment - 1);
    return address;
  }
};

static bool g_traceSyscalls = false;
static const char *getSyscallName(orbis::Thread *thread, int sysno) {
  auto sysvec = thread->tproc->sysent;

  if (sysno >= sysvec->size) {
    return nullptr;
  }

  return orbis::getSysentName(sysvec->table[sysno].call);
}
static void onSysEnter(orbis::Thread *thread, int id, uint64_t *args,
                       int argsCount) {
  if (!g_traceSyscalls) {
    return;
  }
  flockfile(stderr);
  std::fprintf(stderr, "   [%u] ", thread->tid);

  if (auto name = getSyscallName(thread, id)) {
    std::fprintf(stderr, "%s(", name);
  } else {
    std::fprintf(stderr, "sys_%u(", id);
  }

  for (int i = 0; i < argsCount; ++i) {
    if (i != 0) {
      std::fprintf(stderr, ", ");
    }

    std::fprintf(stderr, "%#lx", args[i]);
  }

  std::fprintf(stderr, ")\n");
  funlockfile(stderr);
}

static void onSysExit(orbis::Thread *thread, int id, uint64_t *args,
                      int argsCount, orbis::SysResult result) {
  if (!result.isError() && !g_traceSyscalls) {
    return;
  }

  flockfile(stderr);
  std::fprintf(stderr, "%c: [%u] ", result.isError() ? 'E' : 'S', thread->tid);

  if (auto name = getSyscallName(thread, id)) {
    std::fprintf(stderr, "%s(", name);
  } else {
    std::fprintf(stderr, "sys_%u(", id);
  }

  for (int i = 0; i < argsCount; ++i) {
    if (i != 0) {
      std::fprintf(stderr, ", ");
    }

    std::fprintf(stderr, "%#lx", args[i]);
  }

  std::fprintf(stderr, ") -> Status %d, Value %lx:%lx\n", result.value(),
               thread->retval[0], thread->retval[1]);

  if (result.isError()) {
    thread->where();
  }
  funlockfile(stderr);
}

static void ps4InitDev() {
  auto dmem1 = createDmemCharacterDevice(1);
  orbis::g_context.dmemDevice = dmem1;

  auto dce = createDceCharacterDevice();
  orbis::g_context.dceDevice = dce;

  auto ttyFd = ::open("tty.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
  auto consoleDev = createConsoleCharacterDevice(STDIN_FILENO, ttyFd);
  auto mbus = static_cast<MBusDevice *>(createMBusCharacterDevice());
  auto mbusAv = static_cast<MBusAVDevice *>(createMBusAVCharacterDevice());

  // FIXME: make it configurable
  auto defaultAudioDevice = orbis::knew<AlsaDevice>();
  auto nullAudioDevice = orbis::knew<AudioDevice>();

  auto hdmiAudioDevice = defaultAudioDevice;
  auto analogAudioDevice = nullAudioDevice;
  auto spdifAudioDevice = nullAudioDevice;

  vfs::addDevice("dmem0", createDmemCharacterDevice(0));
  vfs::addDevice("npdrm", createNpdrmCharacterDevice());
  vfs::addDevice("icc_configuration", createIccConfigurationCharacterDevice());
  vfs::addDevice("console", consoleDev);
  vfs::addDevice("camera", createCameraCharacterDevice());
  vfs::addDevice("dmem1", dmem1);
  vfs::addDevice("dmem2", createDmemCharacterDevice(2));
  vfs::addDevice("stdout", consoleDev);
  vfs::addDevice("stderr", consoleDev);
  vfs::addDevice("deci_stdin", consoleDev);
  vfs::addDevice("deci_stdout", consoleDev);
  vfs::addDevice("deci_stderr", consoleDev);
  vfs::addDevice("deci_tty1", consoleDev);
  vfs::addDevice("deci_tty2", consoleDev);
  vfs::addDevice("deci_tty3", consoleDev);
  vfs::addDevice("deci_tty4", consoleDev);
  vfs::addDevice("deci_tty5", consoleDev);
  vfs::addDevice("deci_tty6", consoleDev);
  vfs::addDevice("deci_tty7", consoleDev);
  vfs::addDevice("stdin", consoleDev);
  vfs::addDevice("zero", createZeroCharacterDevice());
  vfs::addDevice("null", createNullCharacterDevice());
  vfs::addDevice("dipsw", createDipswCharacterDevice());
  vfs::addDevice("dce", dce);
  vfs::addDevice("hmd_cmd", createHmdCmdCharacterDevice());
  vfs::addDevice("hmd_snsr", createHmdSnsrCharacterDevice());
  vfs::addDevice("hmd_3da", createHmd3daCharacterDevice());
  vfs::addDevice("hmd_dist", createHmdMmapCharacterDevice());
  vfs::addDevice("hid", createHidCharacterDevice());
  vfs::addDevice("gc", createGcCharacterDevice());
  vfs::addDevice("rng", createRngCharacterDevice());
  vfs::addDevice("sbl_srv", createSblSrvCharacterDevice());
  vfs::addDevice("ajm", createAjmCharacterDevice());
  vfs::addDevice("urandom", createUrandomCharacterDevice());
  vfs::addDevice("mbus", mbus);
  vfs::addDevice("metadbg", createMetaDbgCharacterDevice());
  vfs::addDevice("bt", createBtCharacterDevice());
  vfs::addDevice("xpt0", createXptCharacterDevice());
  vfs::addDevice("cd0", createCdCharacterDevice());
  vfs::addDevice("da0", createHddCharacterDevice(250ull * 1024 * 1024 * 1024));
  vfs::addDevice("da0x0.crypt", createHddCharacterDevice(0x20000000));
  vfs::addDevice("da0x1.crypt", createHddCharacterDevice(0x40000000));
  vfs::addDevice("da0x2", createHddCharacterDevice(0x1000000));
  vfs::addDevice("da0x2.crypt", createHddCharacterDevice(0x1000000));
  vfs::addDevice("da0x3.crypt", createHddCharacterDevice(0x8000000));
  vfs::addDevice("da0x4.crypt", createHddCharacterDevice(0x40000000));
  vfs::addDevice("da0x4b.crypt", createHddCharacterDevice(0x40000000));
  vfs::addDevice("da0x5.crypt", createHddCharacterDevice(0x40000000));
  vfs::addDevice("da0x5b.crypt", createHddCharacterDevice(0x40000000));
  // vfs::addDevice("da0x6x0", createHddCharacterDevice()); // boot log
  vfs::addDevice("da0x6", createHddCharacterDevice(0x200000000));
  vfs::addDevice("da0x6x2.crypt", createHddCharacterDevice(0x200000000));
  vfs::addDevice("da0x8", createHddCharacterDevice(0x40000000));
  vfs::addDevice("da0x8.crypt", createHddCharacterDevice(0x40000000));
  vfs::addDevice("da0x9.crypt", createHddCharacterDevice(0x200000000));
  vfs::addDevice("da0x12.crypt", createHddCharacterDevice(0x180000000));
  vfs::addDevice("da0x13.crypt", createHddCharacterDevice(0));
  vfs::addDevice("da0x14.crypt", createHddCharacterDevice(0x40000000));
  vfs::addDevice("da0x15", createHddCharacterDevice(0));
  vfs::addDevice("da0x15.crypt", createHddCharacterDevice(0x400000000));
  vfs::addDevice("notification0", createNotificationCharacterDevice(0));
  vfs::addDevice("notification1", createNotificationCharacterDevice(1));
  vfs::addDevice("notification2", createNotificationCharacterDevice(2));
  vfs::addDevice("notification3", createNotificationCharacterDevice(3));
  vfs::addDevice("notification4", createNotificationCharacterDevice(4));
  vfs::addDevice("notification5", createNotificationCharacterDevice(5));
  vfs::addDevice("aout0", createAoutCharacterDevice(0, hdmiAudioDevice));
  vfs::addDevice("aout1", createAoutCharacterDevice(1, analogAudioDevice));
  vfs::addDevice("aout2", createAoutCharacterDevice(2, spdifAudioDevice));
  vfs::addDevice("av_control", createAVControlCharacterDevice());
  vfs::addDevice("hdmi", createHDMICharacterDevice());
  vfs::addDevice("mbus_av", mbusAv);
  vfs::addDevice("scanin", createScaninCharacterDevice());
  vfs::addDevice("s3da", createS3DACharacterDevice());
  vfs::addDevice("gbase", createGbaseCharacterDevice());
  vfs::addDevice("devstat", createDevStatCharacterDevice());
  vfs::addDevice("devact", createDevActCharacterDevice());
  vfs::addDevice("devctl", createDevCtlCharacterDevice());
  vfs::addDevice("uvd", createUVDCharacterDevice());
  vfs::addDevice("vce", createVCECharacterDevice());
  vfs::addDevice("evlg1", createEvlgCharacterDevice(ttyFd));
  vfs::addDevice("srtc", createSrtcCharacterDevice());
  vfs::addDevice("sshot", createScreenShotCharacterDevice());
  vfs::addDevice("lvdctl", createLvdCtlCharacterDevice());
  vfs::addDevice("lvd0", createHddCharacterDevice(0x100000000));
  vfs::addDevice("icc_power", createIccPowerCharacterDevice());
  vfs::addDevice("cayman/reg", createCaymanRegCharacterDevice());
  vfs::addDevice("hctrl", createHidCharacterDevice());

  // mbus->emitEvent({
  //     .system = 2,
  //     .eventId = 1,
  //     .deviceId = 0,
  // });

  // mbus->emitEvent({
  //     .system = 9,
  //     .eventId = 1,
  //     .deviceId = 100,
  // });

  mbusAv->emitEvent({
      .system = 9,
      .eventId = 1,
      .deviceId = 100,
  });

  auto shm = createShmDevice();
  orbis::g_context.shmDevice = shm;
  orbis::g_context.blockpoolDevice = createBlockPoolDevice();
}

static void ps4InitFd(orbis::Thread *mainThread) {
  orbis::Ref<orbis::File> stdinFile;
  orbis::Ref<orbis::File> stdoutFile;
  orbis::Ref<orbis::File> stderrFile;
  rx::procOpsTable.open(mainThread, "/dev/stdin", 0, 0, &stdinFile);
  rx::procOpsTable.open(mainThread, "/dev/stdout", 0, 0, &stdoutFile);
  rx::procOpsTable.open(mainThread, "/dev/stderr", 0, 0, &stderrFile);

  mainThread->tproc->fileDescriptors.insert(stdinFile);
  mainThread->tproc->fileDescriptors.insert(stdoutFile);
  mainThread->tproc->fileDescriptors.insert(stderrFile);
}

static orbis::Process *createGuestProcess() {
  auto pid = orbis::g_context.allocatePid() * 10000 + 1;
  return orbis::g_context.createProcess(pid);
}

static orbis::Thread *createGuestThread() {
  auto process = createGuestProcess();
  auto [baseId, thread] = process->threadsMap.emplace();
  thread->tproc = process;
  thread->tid = process->pid + baseId;
  thread->state = orbis::ThreadState::RUNNING;
  return thread;
}

struct ExecEnv {
  std::uint64_t entryPoint;
  std::uint64_t interpBase;
};

int ps4Exec(orbis::Thread *mainThread, ExecEnv execEnv,
            orbis::utils::Ref<orbis::Module> executableModule,
            std::span<std::string> argv, std::span<std::string> envp) {
  const auto stackEndAddress = 0x7'ffff'c000ull;
  const auto stackSize = 0x40000 * 32;
  auto stackStartAddress = stackEndAddress - stackSize;
  mainThread->stackStart =
      vm::map(reinterpret_cast<void *>(stackStartAddress), stackSize,
              vm::kMapProtCpuWrite | vm::kMapProtCpuRead,
              vm::kMapFlagAnonymous | vm::kMapFlagFixed | vm::kMapFlagPrivate |
                  vm::kMapFlagStack);

  mainThread->stackEnd =
      reinterpret_cast<std::byte *>(mainThread->stackStart) + stackSize;

  std::vector<std::uint64_t> argvOffsets;
  std::vector<std::uint64_t> envpOffsets;

  StackWriter stack{reinterpret_cast<std::uint64_t>(mainThread->stackEnd)};

  for (auto &elem : argv) {
    argvOffsets.push_back(stack.pushString(elem.data()));
  }

  argvOffsets.push_back(0);

  for (auto &elem : envp) {
    envpOffsets.push_back(stack.pushString(elem.data()));
  }

  envpOffsets.push_back(0);

  // clang-format off
  std::uint64_t auxv[] = {
      AT_ENTRY, executableModule->entryPoint,
      AT_BASE, execEnv.interpBase,
      AT_PHDR, executableModule->phdrAddress,
      AT_PHENT, sizeof(Elf64_Phdr),
      AT_PHNUM, executableModule->phNum,
      AT_NULL, 0
  };
  // clang-format on

  std::size_t argSize =
      sizeof(std::uint64_t) + sizeof(std::uint64_t) * argvOffsets.size() +
      sizeof(std::uint64_t) * envpOffsets.size() + sizeof(auxv);

  auto sp = stack.alloc(argSize, 32);

  auto arg = reinterpret_cast<std::uint64_t *>(sp);
  *arg++ = argvOffsets.size() - 1;

  for (auto argvOffsets : argvOffsets) {
    *arg++ = argvOffsets;
  }

  for (auto envpOffset : envpOffsets) {
    *arg++ = envpOffset;
  }

  executableModule = {};

  memcpy(arg, auxv, sizeof(auxv));

  auto context = new ucontext_t{};

  context->uc_mcontext.gregs[REG_RDI] = sp;
  context->uc_mcontext.gregs[REG_RSP] = sp;

  // FIXME: should be at guest user space
  context->uc_mcontext.gregs[REG_RDX] =
      reinterpret_cast<std::uint64_t>(+[] { std::printf("At exit\n"); });
  context->uc_mcontext.gregs[REG_RIP] = execEnv.entryPoint;

  mainThread->context = context;
  rx::thread::invoke(mainThread);
  std::abort();
}

struct Ps4ProcessParam {
  orbis::size_t size;
  orbis::uint32_t magic;
  orbis::uint32_t version;
  orbis::uint32_t sdkVersion;
  orbis::uint32_t reserved;
  orbis::ptr<char> processName;
  orbis::ptr<char> userMainThreadName;
  orbis::ptr<orbis::uint> userMainThreadPriority;
  orbis::ptr<orbis::uint> userMainThreadStackSize;
  orbis::ptr<void> libcParam;
};

ExecEnv ps4CreateExecEnv(orbis::Thread *mainThread,
                         const orbis::Ref<orbis::Module> &executableModule,
                         bool isSystem) {
  std::uint64_t interpBase = 0;
  std::uint64_t entryPoint = executableModule->entryPoint;

  if (mainThread->tproc->processParam != nullptr &&
      mainThread->tproc->processParamSize >= sizeof(Ps4ProcessParam)) {
    auto processParam =
        reinterpret_cast<std::byte *>(mainThread->tproc->processParam);

    auto sdkVersion = processParam        //
                      + sizeof(uint64_t)  // size
                      + sizeof(uint32_t)  // magic
                      + sizeof(uint32_t); // entryCount

    mainThread->tproc->sdkVersion = *(uint32_t *)sdkVersion;
  }

  if (orbis::g_context.sdkVersion == 0 && mainThread->tproc->sdkVersion != 0) {
    orbis::g_context.sdkVersion = mainThread->tproc->sdkVersion;
  }
  if (mainThread->tproc->sdkVersion == 0) {
    mainThread->tproc->sdkVersion = orbis::g_context.sdkVersion;
  }

  if (executableModule->interp.empty()) {
    return {.entryPoint = entryPoint, .interpBase = interpBase};
  }

  if (vfs::exists(executableModule->interp, mainThread)) {
    auto loader =
        rx::linker::loadModuleFile(executableModule->interp, mainThread);
    loader->id = mainThread->tproc->modulesMap.insert(loader);
    interpBase = reinterpret_cast<std::uint64_t>(loader->base);
    entryPoint = loader->entryPoint;

    return {.entryPoint = entryPoint, .interpBase = interpBase};
  }

  auto libSceLibcInternal = rx::linker::loadModuleFile(
      "/system/common/lib/libSceLibcInternal.sprx", mainThread);

  if (libSceLibcInternal == nullptr) {
    std::println(stderr, "libSceLibcInternal not found");
    std::abort();
  }

  libSceLibcInternal->id =
      mainThread->tproc->modulesMap.insert(libSceLibcInternal);

  auto libkernel = rx::linker::loadModuleFile(
      (isSystem ? "/system/common/lib/libkernel_sys.sprx"
                : "/system/common/lib/libkernel.sprx"),
      mainThread);

  if (libkernel == nullptr) {
    std::println(stderr, "libkernel not found");
    std::abort();
  }

  for (auto sym : libkernel->symbols) {
    if (sym.id == 0xd2f4e7e480cc53d0) {
      auto address = (uint64_t)libkernel->base + sym.address;
      ::mprotect((void *)rx::alignDown(address, 0x1000),
                 rx::alignUp(sym.size + sym.address, 0x1000), PROT_WRITE);
      std::println("patching sceKernelGetMainSocId");
      struct GetMainSocId : Xbyak::CodeGenerator {
        GetMainSocId(std::uint64_t address, std::uint64_t size)
            : Xbyak::CodeGenerator(size, (void *)address) {
          mov(eax, 0x710f00);
          ret();
        }
      } gen{address, sym.size};

      ::mprotect((void *)rx::alignDown(address, 0x1000),
                 rx::alignUp(sym.size + sym.address, 0x1000),
                 PROT_READ | PROT_EXEC);
      break;
    }
  }

  if (orbis::g_context.fwSdkVersion == 0) {
    auto moduleParam = reinterpret_cast<std::byte *>(libkernel->moduleParam);
    auto fwSdkVersion = moduleParam         //
                        + sizeof(uint64_t)  // size
                        + sizeof(uint64_t); // magic
    orbis::g_context.fwSdkVersion = *(uint32_t *)fwSdkVersion;
    std::printf("fw sdk version: %x\n", orbis::g_context.fwSdkVersion);
  }

  libkernel->id = mainThread->tproc->modulesMap.insert(libkernel);
  interpBase = reinterpret_cast<std::uint64_t>(libkernel->base);
  entryPoint = libkernel->entryPoint;

  return {.entryPoint = entryPoint, .interpBase = interpBase};
}

int ps4Exec(orbis::Thread *mainThread,
            orbis::utils::Ref<orbis::Module> executableModule,
            std::span<std::string> argv, std::span<std::string> envp) {
  auto execEnv = ps4CreateExecEnv(mainThread, executableModule, true);
  return ps4Exec(mainThread, execEnv, std::move(executableModule), argv, envp);
}

static void usage(const char *argv0) {
  std::println("{} [<options>...] <virtual path to elf> [args...]", argv0);
  std::println("  options:");
  std::println("  --version, -v - print version");
  std::println("    -m, --mount <host path> <virtual path>");
  std::println("    -o, --override <original module name> <virtual path to "
               "overriden module>");
  std::println("    --fw <path to firmware root>");
  std::println(
      "    --gpu <index> - specify physical gpu index to use, default is 0");
  std::println("    --disable-cache - disable cache of gpu resources");
  // std::println("    --presenter <window>");
  std::println("    --trace");
}

static orbis::SysResult launchDaemon(orbis::Thread *thread, std::string path,
                                     std::vector<std::string> argv,
                                     std::vector<std::string> envv,
                                     orbis::AppInfo appInfo) {
  auto childPid = orbis::g_context.allocatePid() * 10000 + 1;
  auto flag = orbis::knew<std::atomic<bool>>();
  *flag = false;

  int hostPid = ::fork();

  if (hostPid) {
    while (*flag == false) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    orbis::kfree(flag, sizeof(*flag));
    rx::attachProcess(hostPid);
    return {};
  }

  auto process = orbis::g_context.createProcess(childPid);
  auto logFd = ::open(("log-" + std::to_string(childPid) + ".txt").c_str(),
                      O_CREAT | O_TRUNC | O_WRONLY, 0666);

  dup2(logFd, 1);
  dup2(logFd, 2);

  process->hostPid = ::getpid();
  process->sysent = thread->tproc->sysent;
  process->onSysEnter = thread->tproc->onSysEnter;
  process->onSysExit = thread->tproc->onSysExit;
  process->ops = thread->tproc->ops;
  process->parentProcess = thread->tproc;
  process->appInfo = appInfo;

  process->authInfo = {
      .unk0 = 0x380000000000000f,
      .caps =
          {
              -1ul,
              -1ul,
              -1ul,
              -1ul,
          },
      .attrs =
          {
              0x4000400040000000,
              0x4000000000000000,
              0x0080000000000002,
              0xF0000000FFFF4000,
          },
  };
  process->budgetId = 0;
  process->isInSandbox = false;

  vm::fork(childPid);
  vfs::fork();

  *flag = true;

  auto [baseId, newThread] = process->threadsMap.emplace();
  newThread->tproc = process;
  newThread->tid = process->pid + baseId;
  newThread->state = orbis::ThreadState::RUNNING;
  newThread->context = thread->context;
  newThread->fsBase = thread->fsBase;

  orbis::g_currentThread = newThread;
  thread = orbis::g_currentThread;

  setupSigHandlers();
  rx::thread::initialize();
  rx::thread::setupThisThread();

  ps4InitFd(newThread);

  orbis::Ref<orbis::File> socket;
  createSocket(&socket, "", 1, 1, 0);
  process->fileDescriptors.insert(socket);

  ORBIS_LOG_ERROR(__FUNCTION__, path);

  {
    orbis::Ref<orbis::File> file;
    auto result = vfs::open(path, kOpenFlagReadOnly, 0, &file, thread);
    if (result.isError()) {
      return result;
    }
  }

  vm::reset();

  thread->tproc->nextTlsSlot = 1;
  auto executableModule = rx::linker::loadModuleFile(path, thread);

  executableModule->id = thread->tproc->modulesMap.insert(executableModule);
  thread->tproc->processParam = executableModule->processParam;
  thread->tproc->processParamSize = executableModule->processParamSize;

  g_traceSyscalls = false;

  thread->tproc->event.emit(orbis::kEvFiltProc, orbis::kNoteExec);

  std::thread([&] {
    rx::thread::setupSignalStack();
    rx::thread::setupThisThread();
    ps4Exec(thread, executableModule, argv, envv);
  }).join();
  std::abort();
}

int main(int argc, const char *argv[]) {
  if (argc == 2) {
    if (std::strcmp(argv[1], "-h") == 0 ||
        std::strcmp(argv[1], "--help") == 0) {
      usage(argv[0]);
      return 1;
    }

    if (argv[1] == std::string_view("-v") ||
        argv[1] == std::string_view("--version")) {
      std::printf("v%s\n", rx::getVersion().toString().c_str());
      return 0;
    }
  }

  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  orbis::g_context.deviceEventEmitter = orbis::knew<orbis::EventEmitter>();

  bool enableAudioIpmi = false;
  bool asRoot = false;
  bool isSystem = false;
  bool isSafeMode = false;

  int argIndex = 1;
  while (argIndex < argc) {
    if (argv[argIndex] == std::string_view("--mount") ||
        argv[argIndex] == std::string_view("-m")) {
      if (argc <= argIndex + 2) {
        usage(argv[0]);
        return 1;
      }

      std::println("mounting '{}' to virtual '{}'", argv[argIndex + 1],
                   argv[argIndex + 2]);
      if (!std::filesystem::is_directory(argv[argIndex + 1])) {
        std::println(stderr, "Directory '{}' not exists", argv[argIndex + 1]);
        return 1;
      }

      vfs::mount(argv[argIndex + 2],
                 createHostIoDevice(argv[argIndex + 1], argv[argIndex + 2]));
      argIndex += 3;
      continue;
    }

    if (argv[argIndex] == std::string_view("--fw")) {
      if (argc <= argIndex + 1) {
        usage(argv[0]);
        return 1;
      }

      std::println("mounting firmware '{}'", argv[argIndex + 1]);

      vfs::mount("/", createHostIoDevice(argv[argIndex + 1], "/"));

      argIndex += 2;
      continue;
    }

    if (argv[argIndex] == std::string_view("--trace")) {
      argIndex++;
      g_traceSyscalls = true;
      continue;
    }

    if (argv[argIndex] == std::string_view("--root")) {
      argIndex++;
      asRoot = true;
      continue;
    }

    if (argv[argIndex] == std::string_view("--system")) {
      argIndex++;
      isSystem = true;
      asRoot = true;
      continue;
    }

    if (argv[argIndex] == std::string_view("--safemode")) {
      argIndex++;
      isSafeMode = true;
      asRoot = true;
      continue;
    }

    if (argv[argIndex] == std::string_view("--override") ||
        argv[argIndex] == std::string_view("-o")) {
      if (argc <= argIndex + 2) {
        usage(argv[0]);
        return 1;
      }

      rx::linker::override(argv[argIndex + 1], argv[argIndex + 2]);

      argIndex += 3;
      continue;
    }

    if (argv[argIndex] == std::string_view("--enable-audio") ||
        argv[argIndex] == std::string_view("-a")) {
      argIndex++;
      enableAudioIpmi = true;
      continue;
    }

    if (argv[argIndex] == std::string_view("--disable-cache")) {
      argIndex++;
      rx::g_config.disableGpuCache = true;
      continue;
    }

    if (argv[argIndex] == std::string_view("--debug-gpu")) {
      argIndex++;
      rx::g_config.debugGpu = true;
      continue;
    }

    if (argv[argIndex] == std::string_view("--gpu")) {
      if (argc <= argIndex + 1) {
        usage(argv[0]);
        return 1;
      }

      rx::g_config.gpuIndex = std::atoi(argv[argIndex + 1]);

      argIndex += 2;
      continue;
    }

    if (argv[argIndex] == std::string_view("--validate")) {
      rx::g_config.validateGpu = true;
      argIndex++;
      continue;
    }

    break;
  }

  setupSigHandlers();

  rx::startWatchdog();

  rx::createGpuDevice();
  vfs::initialize();

  std::vector<std::string> guestArgv(argv + argIndex, argv + argc);
  if (guestArgv.empty()) {
    guestArgv.emplace_back("/mini-syscore.elf");
    isSystem = true;
  }

  rx::thread::initialize();

  // vm::printHostStats();
  orbis::g_context.allocatePid();
  auto initProcess = orbis::g_context.createProcess(asRoot ? 1 : 10);
  // pthread_setname_np(pthread_self(), "10.MAINTHREAD");

  vm::initialize(initProcess->pid);

  int status = 0;

  initProcess->sysent = &orbis::ps4_sysvec;
  initProcess->onSysEnter = onSysEnter;
  initProcess->onSysExit = onSysExit;
  initProcess->ops = &rx::procOpsTable;
  initProcess->hostPid = ::getpid();
  initProcess->appInfo = {
      .unk4 = (isSystem ? orbis::slong(0x80000000'00000000) : 0),
  };

  if (isSystem) {
    orbis::g_context.safeMode = isSafeMode ? 1 : 0;
    initProcess->authInfo = {.unk0 = 0x380000000000000f,
                             .caps =
                                 {
                                     -1ul,
                                     -1ul,
                                     -1ul,
                                     -1ul,
                                 },
                             .attrs =
                                 {
                                     0x4000400040000000,
                                     0x4000000000000000,
                                     0x0080000000000002,
                                     0xF0000000FFFF4000,
                                 },
                             .ucred = {
                                 -1ul,
                                 -1ul,
                                 0x3800000000000022,
                                 -1ul,
                                 (1ul << 0x3a),
                                 -1ul,
                                 -1ul,
                             }};
    initProcess->budgetId = 0;
    initProcess->isInSandbox = false;
  } else {
    initProcess->authInfo = {
        .unk0 = 0x3100000000000001,
        .caps =
            {
                0x2000038000000000,
                0x000000000000FF00,
                0x0000000000000000,
                0x0000000000000000,
            },
        .attrs =
            {
                0x4000400040000000,
                0x4000000000000000,
                0x0080000000000002,
                0xF0000000FFFF4000,
            },
    };
    initProcess->budgetId = 1;
    initProcess->isInSandbox = true;
  }

  auto [baseId, mainThread] = initProcess->threadsMap.emplace();
  mainThread->tproc = initProcess;
  mainThread->tid = initProcess->pid + baseId;
  mainThread->state = orbis::ThreadState::RUNNING;
  mainThread->hostTid = ::gettid();
  orbis::g_currentThread = mainThread;

  if (!isSystem && !vfs::exists(guestArgv[0], mainThread) &&
      std::filesystem::exists(guestArgv[0])) {
    std::filesystem::path filePath(guestArgv[0]);
    if (std::filesystem::is_directory(filePath)) {
      filePath /= "eboot.bin";
    }

    vfs::mount("/app0",
               createHostIoDevice(filePath.parent_path().c_str(), "/app0"));
    guestArgv[0] = "/app0" / filePath.filename();
  }

  auto executableModule = rx::linker::loadModuleFile(guestArgv[0], mainThread);

  if (executableModule == nullptr) {
    std::println(stderr, "Failed to open '{}'", guestArgv[0]);
    std::abort();
  }

  executableModule->id = initProcess->modulesMap.insert(executableModule);
  initProcess->processParam = executableModule->processParam;
  initProcess->processParamSize = executableModule->processParamSize;

  if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
            (void *)0x100'0000'0000, ~0ull - 0x100'0000'0000, nullptr)) {
    perror("prctl failed\n");
    return 1;
  }

  if (executableModule->type != rx::linker::kElfTypeSceDynExec &&
      executableModule->type != rx::linker::kElfTypeSceExec &&
      executableModule->type != rx::linker::kElfTypeExec) {
    std::println(stderr, "Unexpected executable type");
    status = 1;
    return 1;
  }

  ps4InitDev();
  ps4InitFd(mainThread);

  auto execEnv = ps4CreateExecEnv(mainThread, executableModule, isSystem);

  // data transfer mode
  // 0 - normal
  // 1 - source
  // 2 - ?
  orbis::g_context.regMgrInt[0x2110000] = 0;

  orbis::g_context.regMgrInt[0x20b0000] = 1; // prefer X
  orbis::g_context.regMgrInt[0x2020000] = 1; // region

  // orbis::g_context.regMgrInt[0x2130000] = 0x1601;
  orbis::g_context.regMgrInt[0x2130000] = 0;
  orbis::g_context.regMgrInt[0x73800200] = 1;
  orbis::g_context.regMgrInt[0x73800300] = 0;
  orbis::g_context.regMgrInt[0x73800400] = 0;
  orbis::g_context.regMgrInt[0x73800500] = 0; // enable log

  // user settings
  orbis::g_context.regMgrInt[0x7800100] = 0;
  orbis::g_context.regMgrInt[0x7810100] = 0;
  orbis::g_context.regMgrInt[0x7820100] = 0;
  orbis::g_context.regMgrInt[0x7830100] = 0;
  orbis::g_context.regMgrInt[0x7840100] = 0;
  orbis::g_context.regMgrInt[0x7850100] = 0;
  orbis::g_context.regMgrInt[0x7860100] = 0;
  orbis::g_context.regMgrInt[0x7870100] = 0;
  orbis::g_context.regMgrInt[0x7880100] = 0;
  orbis::g_context.regMgrInt[0x7890100] = 0;
  orbis::g_context.regMgrInt[0x78a0100] = 0;
  orbis::g_context.regMgrInt[0x78b0100] = 0;
  orbis::g_context.regMgrInt[0x78c0100] = 0;
  orbis::g_context.regMgrInt[0x78d0100] = 0;
  orbis::g_context.regMgrInt[0x78e0100] = 0;
  orbis::g_context.regMgrInt[0x78f0100] = 0;

  orbis::g_context.regMgrInt[0x2040000] = 0; // do not require initial setup
  orbis::g_context.regMgrInt[0x2800600] = 0; // IDU version
  orbis::g_context.regMgrInt[0x2860100] = 0; // IDU mode
  orbis::g_context.regMgrInt[0x2860300] = 0; // Arcade mode
  orbis::g_context.regMgrInt[0x7010000] = 0; // auto login
  orbis::g_context.regMgrInt[0x9010000] = 0; // video out color effect

  if (!isSystem) {
    ipmi::createMiniSysCoreObjects(initProcess);
    ipmi::createSysAvControlObjects(initProcess);
    ipmi::createSysCoreObjects(initProcess);
    ipmi::createGnmCompositorObjects(initProcess);
    ipmi::createShellCoreObjects(initProcess);
    if (enableAudioIpmi) {
      ipmi::createAudioSystemObjects(initProcess);
    }

    // ?
    ipmi::createIpmiServer(initProcess, "SceCdlgRichProf");
    ipmi::createIpmiServer(initProcess, "SceRemoteplayIpc");
    ipmi::createIpmiServer(initProcess, "SceGlsIpc");
    ipmi::createIpmiServer(initProcess, "SceImeService");
    ipmi::createIpmiServer(initProcess, "SceErrorDlgServ");

    ipmi::createEventFlag("SceNpTusIpc_0000000a", 0x120, 0);
    ipmi::createSemaphore("SceLncSuspendBlock00000000", 0x101, 1, 1);
    ipmi::createSemaphore("SceNpPlusLogger 0", 0x101, 0, 0x7fffffff);

    initProcess->cwd = "/app0/";

    if (!enableAudioIpmi) {
      launchDaemon(mainThread, "/system/sys/orbis_audiod.elf",
                   {"/system/sys/orbis_audiod.elf"}, {},
                   {
                       .titleId = "NPXS20973",
                       .unk4 = orbis::slong(0x80000000'00000000),
                   });
      // confirmed to work and known method of initialization since 5.05
      // version
      if (orbis::g_context.fwSdkVersion >= 0x5050000) {
        auto fakeIpmiThread = createGuestThread();
        ipmi::audioIpmiClient =
            ipmi::createIpmiClient(fakeIpmiThread, "SceSysAudioSystemIpc");
        // HACK: here is a bug in audiod because we send this very early and
        // audiod has time to reset the state due to initialization so we wait
        // for a second, during this time audiod should have time to
        // initialize on most systems
        std::this_thread::sleep_for(std::chrono::seconds(1));
        struct Data1 {
          int32_t pid = 0;
          int32_t someSwitch = 0x14; // 0x14 for init, 0x19 for mute
          int32_t someFlag = 0;
        } data1;
        data1.pid = fakeIpmiThread->tproc->pid;
        struct Data2 {
          void *unk0 = 0;
          int32_t unk1 = 0x105;
          int32_t unk2 = 0x10000;
          int64_t unk3 = 0;
          int32_t unk4 = 0;
          int32_t unk5 = 0;
          int32_t unk6 = 0;
          int64_t unk7 = 0;
          int32_t unk8 = 0x2;
          char unk9[24]{0};
        } data2;
        std::uint32_t method = orbis::g_context.fwSdkVersion >= 0x8000000
                                   ? 0x1234002c
                                   : 0x1234002b;
        ipmi::audioIpmiClient.sendSyncMessage(method, data1, data2);
      }
    }
  }

  status =
      ps4Exec(mainThread, execEnv, std::move(executableModule), guestArgv, {});

  vm::deinitialize();
  rx::thread::deinitialize();

  return status;
}
