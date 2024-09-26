#include "AudioOut.hpp"
#include "amdgpu/bridge/bridge.hpp"
#include "backtrace.hpp"
#include "bridge.hpp"
#include "io-device.hpp"
#include "io-devices.hpp"
#include "iodev/mbus.hpp"
#include "iodev/mbus_av.hpp"
#include "linker.hpp"
#include "ops.hpp"
#include "orbis/utils/Logs.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include "xbyak/xbyak.h"
#include <orbis/utils/Rc.hpp>
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
#include <tuple>
#include <ucontext.h>

#include <atomic>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <sstream>
#include <unordered_map>

static int g_gpuPid;
extern bool allowMonoDebug;

template <typename T> std::vector<std::byte> toBytes(const T &value) {
  std::vector<std::byte> result(sizeof(T));
  std::memcpy(result.data(), &value, sizeof(value));
  return result;
}

__attribute__((no_stack_protector)) static void
handle_signal(int sig, siginfo_t *info, void *ucontext) {
  if (auto hostFs = _readgsbase_u64()) {
    _writefsbase_u64(hostFs);
  }

  auto signalAddress = reinterpret_cast<std::uintptr_t>(info->si_addr);

  if (orbis::g_currentThread != nullptr &&
      orbis::g_currentThread->tproc->vmId >= 0 && sig == SIGSEGV &&
      signalAddress >= 0x40000 && signalAddress < 0x100'0000'0000) {
    auto vmid = orbis::g_currentThread->tproc->vmId;
    auto ctx = reinterpret_cast<ucontext_t *>(ucontext);
    bool isWrite = (ctx->uc_mcontext.gregs[REG_ERR] & 0x2) != 0;
    auto origVmProt = rx::vm::getPageProtection(signalAddress);
    int prot = 0;
    auto page = signalAddress / amdgpu::bridge::kHostPageSize;

    if (origVmProt & rx::vm::kMapProtCpuRead) {
      prot |= PROT_READ;
    }
    if (origVmProt & rx::vm::kMapProtCpuWrite) {
      prot |= PROT_WRITE;
    }
    if (origVmProt & rx::vm::kMapProtCpuExec) {
      prot |= PROT_EXEC;
    }

    if (prot & (isWrite ? PROT_WRITE : PROT_READ)) {
      auto bridge = rx::bridge.header;

      while (true) {
        auto flags =
            bridge->cachePages[vmid][page].load(std::memory_order::relaxed);

        if ((flags & amdgpu::bridge::kPageReadWriteLock) != 0) {
          if ((flags & amdgpu::bridge::kPageLazyLock) != 0) {
            if (std::uint32_t gpuCommand = 0;
                !bridge->gpuCacheCommand[vmid].compare_exchange_weak(gpuCommand,
                                                                     page)) {
              continue;
            }

            while (!bridge->cachePages[vmid][page].compare_exchange_weak(
                flags, flags & ~amdgpu::bridge::kPageLazyLock,
                std::memory_order::relaxed)) {
            }
          }
          continue;
        }

        if ((flags & amdgpu::bridge::kPageWriteWatch) == 0) {
          break;
        }

        if (!isWrite) {
          prot &= ~PROT_WRITE;
          break;
        }

        if (bridge->cachePages[vmid][page].compare_exchange_weak(
                flags, amdgpu::bridge::kPageInvalidated,
                std::memory_order::relaxed)) {
          break;
        }
      }

      if (::mprotect((void *)(page * amdgpu::bridge::kHostPageSize),
                     amdgpu::bridge::kHostPageSize, prot)) {
        std::perror("cache reprotection error");
        std::abort();
      }

      _writefsbase_u64(orbis::g_currentThread->fsBase);
      return;
    }

    std::fprintf(stderr, "SIGSEGV, address %lx, access %s, prot %s\n",
                 signalAddress, isWrite ? "write" : "read",
                 rx::vm::mapProtToString(origVmProt).c_str());
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
    int len = snprintf(buf, sizeof(buf), " [%s] %u: Signal address=%p\n",
                       orbis::g_currentThread ? "guest" : "host",
                       orbis::g_currentThread ? orbis::g_currentThread->tid
                                              : ::gettid(),
                       info->si_addr);
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

  if (sigaction(SIGINT, &act, NULL)) {
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

  auto ttyFd = ::open("tty.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
  auto consoleDev = createConsoleCharacterDevice(STDIN_FILENO, ttyFd);
  auto mbus = static_cast<MBusDevice *>(createMBusCharacterDevice());
  auto mbusAv = static_cast<MBusAVDevice *>(createMBusAVCharacterDevice());

  rx::vfs::addDevice("dmem0", createDmemCharacterDevice(0));
  rx::vfs::addDevice("npdrm", createNpdrmCharacterDevice());
  rx::vfs::addDevice("icc_configuration",
                     createIccConfigurationCharacterDevice());
  rx::vfs::addDevice("console", consoleDev);
  rx::vfs::addDevice("camera", createCameraCharacterDevice());
  rx::vfs::addDevice("dmem1", dmem1);
  rx::vfs::addDevice("dmem2", createDmemCharacterDevice(2));
  rx::vfs::addDevice("stdout", consoleDev);
  rx::vfs::addDevice("stderr", consoleDev);
  rx::vfs::addDevice("deci_stdin", consoleDev);
  rx::vfs::addDevice("deci_stdout", consoleDev);
  rx::vfs::addDevice("deci_stderr", consoleDev);
  rx::vfs::addDevice("deci_tty1", consoleDev);
  rx::vfs::addDevice("deci_tty2", consoleDev);
  rx::vfs::addDevice("deci_tty3", consoleDev);
  rx::vfs::addDevice("deci_tty4", consoleDev);
  rx::vfs::addDevice("deci_tty5", consoleDev);
  rx::vfs::addDevice("deci_tty6", consoleDev);
  rx::vfs::addDevice("deci_tty7", consoleDev);
  rx::vfs::addDevice("stdin", consoleDev);
  rx::vfs::addDevice("zero", createZeroCharacterDevice());
  rx::vfs::addDevice("null", createNullCharacterDevice());
  rx::vfs::addDevice("dipsw", createDipswCharacterDevice());
  rx::vfs::addDevice("dce", createDceCharacterDevice());
  rx::vfs::addDevice("hmd_cmd", createHmdCmdCharacterDevice());
  rx::vfs::addDevice("hmd_snsr", createHmdSnsrCharacterDevice());
  rx::vfs::addDevice("hmd_3da", createHmd3daCharacterDevice());
  rx::vfs::addDevice("hmd_dist", createHmdMmapCharacterDevice());
  rx::vfs::addDevice("hid", createHidCharacterDevice());
  rx::vfs::addDevice("gc", createGcCharacterDevice());
  rx::vfs::addDevice("rng", createRngCharacterDevice());
  rx::vfs::addDevice("sbl_srv", createSblSrvCharacterDevice());
  rx::vfs::addDevice("ajm", createAjmCharacterDevice());
  rx::vfs::addDevice("urandom", createUrandomCharacterDevice());
  rx::vfs::addDevice("mbus", mbus);
  rx::vfs::addDevice("metadbg", createMetaDbgCharacterDevice());
  rx::vfs::addDevice("bt", createBtCharacterDevice());
  rx::vfs::addDevice("xpt0", createXptCharacterDevice());
  rx::vfs::addDevice("cd0", createCdCharacterDevice());
  rx::vfs::addDevice("da0",
                     createHddCharacterDevice(250ull * 1024 * 1024 * 1024));
  rx::vfs::addDevice("da0x0.crypt", createHddCharacterDevice(0x20000000));
  rx::vfs::addDevice("da0x1.crypt", createHddCharacterDevice(0x40000000));
  rx::vfs::addDevice("da0x2", createHddCharacterDevice(0x1000000));
  rx::vfs::addDevice("da0x2.crypt", createHddCharacterDevice(0x1000000));
  rx::vfs::addDevice("da0x3.crypt", createHddCharacterDevice(0x8000000));
  rx::vfs::addDevice("da0x4.crypt", createHddCharacterDevice(0x40000000));
  rx::vfs::addDevice("da0x4b.crypt", createHddCharacterDevice(0x40000000));
  rx::vfs::addDevice("da0x5.crypt", createHddCharacterDevice(0x40000000));
  rx::vfs::addDevice("da0x5b.crypt", createHddCharacterDevice(0x40000000));
  // rx::vfs::addDevice("da0x6x0", createHddCharacterDevice()); // boot log
  rx::vfs::addDevice("da0x6", createHddCharacterDevice(0x200000000));
  rx::vfs::addDevice("da0x6x2.crypt", createHddCharacterDevice(0x200000000));
  rx::vfs::addDevice("da0x8", createHddCharacterDevice(0x40000000));
  rx::vfs::addDevice("da0x8.crypt", createHddCharacterDevice(0x40000000));
  rx::vfs::addDevice("da0x9.crypt", createHddCharacterDevice(0x200000000));
  rx::vfs::addDevice("da0x12.crypt", createHddCharacterDevice(0x180000000));
  rx::vfs::addDevice("da0x13.crypt", createHddCharacterDevice(0));
  rx::vfs::addDevice("da0x14.crypt", createHddCharacterDevice(0x40000000));
  rx::vfs::addDevice("da0x15", createHddCharacterDevice(0));
  rx::vfs::addDevice("da0x15.crypt", createHddCharacterDevice(0x400000000));
  rx::vfs::addDevice("notification0", createNotificationCharacterDevice(0));
  rx::vfs::addDevice("notification1", createNotificationCharacterDevice(1));
  rx::vfs::addDevice("notification2", createNotificationCharacterDevice(2));
  rx::vfs::addDevice("notification3", createNotificationCharacterDevice(3));
  rx::vfs::addDevice("notification4", createNotificationCharacterDevice(4));
  rx::vfs::addDevice("notification5", createNotificationCharacterDevice(5));
  rx::vfs::addDevice("aout0", createAoutCharacterDevice());
  rx::vfs::addDevice("aout1", createAoutCharacterDevice());
  rx::vfs::addDevice("aout2", createAoutCharacterDevice());
  rx::vfs::addDevice("av_control", createAVControlCharacterDevice());
  rx::vfs::addDevice("hdmi", createHDMICharacterDevice());
  rx::vfs::addDevice("mbus_av", mbusAv);
  rx::vfs::addDevice("scanin", createScaninCharacterDevice());
  rx::vfs::addDevice("s3da", createS3DACharacterDevice());
  rx::vfs::addDevice("gbase", createGbaseCharacterDevice());
  rx::vfs::addDevice("devstat", createDevStatCharacterDevice());
  rx::vfs::addDevice("devact", createDevActCharacterDevice());
  rx::vfs::addDevice("devctl", createDevCtlCharacterDevice());
  rx::vfs::addDevice("uvd", createUVDCharacterDevice());
  rx::vfs::addDevice("vce", createVCECharacterDevice());
  rx::vfs::addDevice("evlg1", createEvlgCharacterDevice(ttyFd));
  rx::vfs::addDevice("srtc", createSrtcCharacterDevice());
  rx::vfs::addDevice("sshot", createScreenShotCharacterDevice());
  rx::vfs::addDevice("lvdctl", createLvdCtlCharacterDevice());
  rx::vfs::addDevice("lvd0", createHddCharacterDevice(0x100000000));
  rx::vfs::addDevice("icc_power", createIccPowerCharacterDevice());
  rx::vfs::addDevice("cayman/reg", createCaymanRegCharacterDevice());
  rx::vfs::addDevice("hctrl", createHidCharacterDevice());

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
  rx::vfs::addDevice("shm", shm);
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
      rx::vm::map(reinterpret_cast<void *>(stackStartAddress), stackSize,
                  rx::vm::kMapProtCpuWrite | rx::vm::kMapProtCpuRead,
                  rx::vm::kMapFlagAnonymous | rx::vm::kMapFlagFixed |
                      rx::vm::kMapFlagPrivate | rx::vm::kMapFlagStack);

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
                         orbis::Ref<orbis::Module> executableModule,
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

  if (rx::vfs::exists(executableModule->interp, mainThread)) {
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
    std::fprintf(stderr, "libSceLibcInternal not found\n");
    std::abort();
  }

  libSceLibcInternal->id =
      mainThread->tproc->modulesMap.insert(libSceLibcInternal);

  auto libkernel = rx::linker::loadModuleFile(
      (isSystem ? "/system/common/lib/libkernel_sys.sprx"
                : "/system/common/lib/libkernel.sprx"),
      mainThread);

  if (libkernel == nullptr) {
    std::fprintf(stderr, "libkernel not found\n");
    std::abort();
  }

  for (auto sym : libkernel->symbols) {
    if (sym.id == 0xd2f4e7e480cc53d0) {
      auto address = (uint64_t)libkernel->base + sym.address;
      ::mprotect((void *)rx::alignDown(address, 0x1000),
                 rx::alignUp(sym.size + sym.address, 0x1000), PROT_WRITE);
      std::printf("patching sceKernelGetMainSocId\n");
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
  std::printf("%s [<options>...] <virtual path to elf> [args...]\n", argv0);
  std::printf("  options:\n");
  std::printf("  --version, -v - print version\n");
  std::printf("    -m, --mount <host path> <virtual path>\n");
  std::printf("    -a, --enable-audio\n");
  std::printf("    -o, --override <original module name> <virtual path to "
              "overriden module>\n");
  std::printf("    --trace\n");
}

static std::filesystem::path getSelfDir() {
  char path[PATH_MAX];
  int len = ::readlink("/proc/self/exe", path, sizeof(path));
  if (len < 0 || len >= sizeof(path)) {
    // TODO
    return std::filesystem::current_path();
  }

  return std::filesystem::path(path).parent_path();
}

static bool isRpcsxGpuPid(int pid) {
  if (pid <= 0 || ::kill(pid, 0) != 0) {
    return false;
  }

  char path[PATH_MAX];
  std::string procPath = "/proc/" + std::to_string(pid) + "/exe";
  auto len = ::readlink(procPath.c_str(), path, sizeof(path));

  if (len < 0 || len >= std::size(path)) {
    return false;
  }

  path[len] = 0;

  std::printf("filename is '%s'\n",
              std::filesystem::path(path).filename().c_str());

  return std::filesystem::path(path).filename().string().starts_with(
      "rpcsx-gpu");
}
static void runRpcsxGpu() {
  const char *cmdBufferName = "/rpcsx-gpu-cmds";
  amdgpu::bridge::BridgeHeader *bridgeHeader =
      amdgpu::bridge::openShmCommandBuffer(cmdBufferName);

  if (bridgeHeader != nullptr && bridgeHeader->pullerPid > 0 &&
      isRpcsxGpuPid(bridgeHeader->pullerPid)) {
    bridgeHeader->pusherPid = ::getpid();
    g_gpuPid = bridgeHeader->pullerPid;
    rx::bridge.header = bridgeHeader;
    return;
  }

  std::printf("Starting rpcsx-gpu\n");

  if (bridgeHeader == nullptr) {
    bridgeHeader = amdgpu::bridge::createShmCommandBuffer(cmdBufferName);
  }
  bridgeHeader->pusherPid = ::getpid();
  rx::bridge.header = bridgeHeader;

  auto rpcsxGpuPath = getSelfDir() / "rpcsx-gpu";

  if (!std::filesystem::is_regular_file(rpcsxGpuPath)) {
    std::printf("failed to find rpcsx-gpu, continue without GPU emulation\n");
    return;
  }

  g_gpuPid = ::fork();

  if (g_gpuPid == 0) {
    // TODO
    const char *argv[] = {rpcsxGpuPath.c_str(), nullptr};

    ::execv(rpcsxGpuPath.c_str(), const_cast<char **>(argv));
  }
}

static orbis::Semaphore *createSemaphore(std::string_view name, uint32_t attrs,
                                         uint64_t initCount,
                                         uint64_t maxCount) {
  auto result =
      orbis::g_context
          .createSemaphore(orbis::kstring(name), attrs, initCount, maxCount)
          .first;
  std::memcpy(result->name, name.data(), name.size());
  result->name[name.size()] = 0;
  return result;
}

static orbis::EventFlag *createEventFlag(std::string_view name, uint32_t attrs,
                                         uint64_t initPattern) {
  return orbis::g_context
      .createEventFlag(orbis::kstring(name), attrs, initPattern)
      .first;
}

static void createShm(const char *name, uint32_t flags, uint32_t mode,
                      uint64_t size) {
  orbis::Ref<orbis::File> shm;
  auto shmDevice = orbis::g_context.shmDevice.staticCast<IoDevice>();
  shmDevice->open(&shm, name, flags, mode, nullptr);
  shm->ops->truncate(shm.get(), size, nullptr);
}

struct IpmiServer {
  orbis::Ref<orbis::IpmiServer> serverImpl;

  std::unordered_map<std::uint32_t,
                     std::function<orbis::ErrorCode(
                         orbis::IpmiSession &session, std::int32_t &errorCode,
                         std::vector<std::vector<std::byte>> &outData,
                         const std::vector<std::span<std::byte>> &inData)>>
      syncMethods;
  std::unordered_map<std::uint32_t,
                     std::function<orbis::ErrorCode(
                         orbis::IpmiSession &session, std::int32_t &errorCode,
                         std::vector<std::vector<std::byte>> &outData,
                         const std::vector<std::span<std::byte>> &inData)>>
      asyncMethods;
  std::vector<std::vector<std::byte>> messages;

  IpmiServer &addSyncMethodStub(
      std::uint32_t methodId,
      std::function<std::int32_t()> handler = [] -> std::int32_t {
        return 0;
      }) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (!outData.empty()) {
        return orbis::ErrorCode::INVAL;
      }

      errorCode = handler();
      return {};
    };
    return *this;
  }

  IpmiServer &addSyncMethod(
      std::uint32_t methodId,
      std::function<std::int32_t(void *out, std::uint64_t &outSize)> handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (outData.size() < 1) {
        return orbis::ErrorCode::INVAL;
      }

      std::uint64_t size = outData[0].size();
      errorCode = handler(outData[0].data(), size);
      outData[0].resize(size);
      return {};
    };
    return *this;
  }

  template <typename T>
  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<std::int32_t(void *out, std::uint64_t &outSize,
                                           const T &param)>
                    handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (outData.size() != 1 || inData.size() != 1) {
        return orbis::ErrorCode::INVAL;
      }

      if (inData[0].size() != sizeof(T)) {
        return orbis::ErrorCode::INVAL;
      }

      std::uint64_t size = outData[0].size();
      errorCode = handler(outData[0].data(), size,
                          *reinterpret_cast<T *>(inData[0].data()));
      outData[0].resize(size);
      return {};
    };
    return *this;
  }

  template <typename OutT, typename InT>
  IpmiServer &addSyncMethod(
      std::uint32_t methodId,
      std::function<std::int32_t(OutT &out, const InT &param)> handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (outData.size() != 1 || inData.size() != 1) {
        return orbis::ErrorCode::INVAL;
      }

      if (inData[0].size() != sizeof(InT)) {
        return orbis::ErrorCode::INVAL;
      }
      if (outData[0].size() < sizeof(OutT)) {
        return orbis::ErrorCode::INVAL;
      }

      OutT out;
      errorCode = handler(out, *reinterpret_cast<InT *>(inData[0].data()));
      std::memcpy(outData[0].data(), &out, sizeof(out));
      outData[0].resize(sizeof(OutT));
      return {};
    };
    return *this;
  }

  template <typename T>
  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<std::int32_t(const T &param)> handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (inData.size() != 1 || !outData.empty()) {
        return orbis::ErrorCode::INVAL;
      }

      if (inData[0].size() != sizeof(T)) {
        return orbis::ErrorCode::INVAL;
      }

      errorCode = handler(*reinterpret_cast<T *>(inData[0].data()));
      return {};
    };
    return *this;
  }

  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<std::int32_t(
                    std::vector<std::vector<std::byte>> &outData,
                    const std::vector<std::span<std::byte>> &inData)>
                    handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      errorCode = handler(outData, inData);
      return {};
    };
    return *this;
  }

  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<orbis::ErrorCode(
                    std::vector<std::vector<std::byte>> &outData,
                    const std::vector<std::span<std::byte>> &inData)>
                    handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode { return handler(outData, inData); };
    return *this;
  }

  IpmiServer &
  addAsyncMethod(std::uint32_t methodId,
                 std::function<orbis::ErrorCode(
                     orbis::IpmiSession &session,
                     std::vector<std::vector<std::byte>> &outData,
                     const std::vector<std::span<std::byte>> &inData)>
                     handler) {
    asyncMethods[methodId] =
        [=](orbis::IpmiSession &session, std::int32_t &errorCode,
            std::vector<std::vector<std::byte>> &outData,
            const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode { return handler(session, outData, inData); };
    return *this;
  }

  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<orbis::ErrorCode(
                    orbis::IpmiSession &session,
                    std::vector<std::vector<std::byte>> &outData,
                    const std::vector<std::span<std::byte>> &inData)>
                    handler) {
    asyncMethods[methodId] =
        [=](orbis::IpmiSession &session, std::int32_t &errorCode,
            std::vector<std::vector<std::byte>> &outData,
            const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode { return handler(session, outData, inData); };
    return *this;
  }

  template <typename T> IpmiServer &sendMsg(const T &data) {
    std::vector<std::byte> message(sizeof(T));
    std::memcpy(message.data(), &data, sizeof(T));
    messages.push_back(std::move(message));
    return *this;
  }

  orbis::ErrorCode handle(orbis::IpmiSession *session,
                          orbis::IpmiAsyncMessageHeader *message) {
    std::vector<std::span<std::byte>> inData;
    std::vector<std::vector<std::byte>> outData;
    auto bufLoc = std::bit_cast<std::byte *>(message + 1);

    for (unsigned i = 0; i < message->numInData; ++i) {
      auto size = *std::bit_cast<orbis::uint *>(bufLoc);
      bufLoc += sizeof(orbis::uint);
      inData.push_back({bufLoc, size});
      bufLoc += size;
    }

    orbis::IpmiClient::AsyncResponse response;
    response.methodId = message->methodId + 1;
    response.errorCode = 0;
    orbis::ErrorCode result{};

    if (auto it = asyncMethods.find(message->methodId);
        it != asyncMethods.end()) {
      auto &handler = it->second;

      result = handler(*session, response.errorCode, outData, inData);
    } else {
      std::fprintf(stderr, "Unimplemented async method %s::%x(inBufCount=%u)\n",
                   session->server->name.c_str(), message->methodId,
                   message->numInData);

      for (auto in : inData) {
        std::fprintf(stderr, "in %zu\n", in.size());
        rx::hexdump(in);
      }
    }

    for (auto out : outData) {
      response.data.push_back({out.data(), out.data() + out.size()});
    }

    std::lock_guard clientLock(session->client->mutex);
    session->client->asyncResponses.push_front(std::move(response));
    std::fprintf(stderr, "%s:%x: sending async response\n",
                 session->client->name.c_str(), message->methodId);
    session->client->asyncResponseCv.notify_all(session->client->mutex);
    return result;
  }

  orbis::ErrorCode handle(orbis::IpmiSession *session,
                          orbis::IpmiServer::Packet &packet,
                          orbis::IpmiSyncMessageHeader *message) {
    std::size_t inBufferOffset = 0;
    auto bufLoc = std::bit_cast<std::byte *>(message + 1);
    std::vector<std::span<std::byte>> inData;
    std::vector<std::vector<std::byte>> outData;
    for (unsigned i = 0; i < message->numInData; ++i) {
      auto size = *std::bit_cast<orbis::uint *>(bufLoc);
      bufLoc += sizeof(orbis::uint);
      inData.push_back({bufLoc, size});
      bufLoc += size;
    }

    for (unsigned i = 0; i < message->numOutData; ++i) {
      auto size = *std::bit_cast<orbis::uint *>(bufLoc);
      bufLoc += sizeof(orbis::uint);
      outData.push_back(std::vector<std::byte>(size));
    }

    orbis::IpmiSession::SyncResponse response;
    response.errorCode = 0;
    orbis::ErrorCode result{};

    if (auto it = syncMethods.find(message->methodId);
        it != syncMethods.end()) {
      auto &handler = it->second;

      result = handler(*session, response.errorCode, outData, inData);
    } else {
      std::fprintf(
          stderr,
          "Unimplemented sync method %s::%x(inBufCount=%u, outBufCount=%u)\n",
          session->server->name.c_str(), message->methodId, message->numInData,
          message->numOutData);

      for (auto in : inData) {
        std::fprintf(stderr, "in %zu\n", in.size());
        rx::hexdump(in);
      }

      for (auto out : outData) {
        std::fprintf(stderr, "out %zx\n", out.size());
      }

      for (auto out : outData) {
        std::memset(out.data(), 0, out.size());
      }
      // TODO:
      // response.errorCode = message->numOutData == 0 ||
      //                      (message->numOutData == 1 && outData[0].empty())
      //                  ? 0
      //                  : -1,
    }

    response.callerTid = packet.clientTid;
    for (auto out : outData) {
      response.data.push_back({out.data(), out.data() + out.size()});
    }

    std::lock_guard lock(session->mutex);
    session->syncResponses.push_front(std::move(response));
    session->responseCv.notify_all(session->mutex);

    return result;
  }
};

static IpmiServer &createIpmiServer(orbis::Process *process, const char *name) {
  orbis::IpmiCreateServerConfig config{};
  orbis::Ref<orbis::IpmiServer> serverImpl;
  orbis::ipmiCreateServer(process, nullptr, name, config, serverImpl);
  auto server = std::make_shared<IpmiServer>();
  server->serverImpl = serverImpl;

  std::thread{[server, serverImpl, name] {
    pthread_setname_np(pthread_self(), name);
    while (true) {
      orbis::IpmiServer::Packet packet;
      {
        std::lock_guard lock(serverImpl->mutex);

        while (serverImpl->packets.empty()) {
          serverImpl->receiveCv.wait(serverImpl->mutex);
        }

        packet = std::move(serverImpl->packets.front());
        serverImpl->packets.pop_front();
      }

      if (packet.info.type == 1) {
        std::lock_guard serverLock(serverImpl->mutex);

        for (auto it = serverImpl->connectionRequests.begin();
             it != serverImpl->connectionRequests.end(); ++it) {
          auto &conReq = *it;
          std::lock_guard clientLock(conReq.client->mutex);
          if (conReq.client->session != nullptr) {
            continue;
          }

          auto session = orbis::knew<orbis::IpmiSession>();
          if (session == nullptr) {
            break;
          }

          session->client = conReq.client;
          session->server = serverImpl;
          conReq.client->session = session;

          for (auto &message : server->messages) {
            conReq.client->messageQueues[0].messages.push_back(
                orbis::kvector<std::byte>(message.data(),
                                          message.data() + message.size()));
          }

          conReq.client->connectionStatus = 0;
          conReq.client->sessionCv.notify_all(conReq.client->mutex);
          conReq.client->connectCv.notify_all(conReq.client->mutex);
          break;
        }

        continue;
      }

      if ((packet.info.type & ~0x8010) == 0x41) {
        auto msgHeader = std::bit_cast<orbis::IpmiSyncMessageHeader *>(
            packet.message.data());
        auto process = orbis::g_context.findProcessById(msgHeader->pid);
        if (process == nullptr) {
          continue;
        }
        auto client = orbis::g_context.ipmiMap.get(packet.info.clientKid)
                          .cast<orbis::IpmiClient>();
        if (client == nullptr) {
          continue;
        }
        auto session = client->session;
        if (session == nullptr) {
          continue;
        }

        server->handle(client->session.get(), packet, msgHeader);
        packet = {};
        continue;
      }

      if ((packet.info.type & ~0x10) == 0x43) {
        auto msgHeader = (orbis::IpmiAsyncMessageHeader *)packet.message.data();
        auto process = orbis::g_context.findProcessById(msgHeader->pid);
        if (process == nullptr) {
          continue;
        }
        auto client = orbis::g_context.ipmiMap.get(packet.info.clientKid)
                          .cast<orbis::IpmiClient>();
        if (client == nullptr) {
          continue;
        }
        auto session = client->session;
        if (session == nullptr) {
          continue;
        }

        server->handle(client->session.get(), msgHeader);
        continue;
      }

      std::fprintf(stderr, "IPMI: Unhandled packet %s::%u\n",
                   serverImpl->name.c_str(), packet.info.type);
    }
  }}.detach();

  return *server;
}

static void createMiniSysCoreObjects(orbis::Process *process) {
  createEventFlag("SceBootStatusFlags", 0x121, ~0ull);
}

static void createSysAvControlObjects(orbis::Process *process) {
  createIpmiServer(process, "SceAvSettingIpc");

  createIpmiServer(process, "SceAvCaptureIpc");
  createEventFlag("SceAvCaptureIpc", 0x121, 0);
  createEventFlag("SceAvSettingEvf", 0x121, 0xffff00000000);

  createShm("/SceAvSetting", 0xa02, 0x1a4, 4096);
}

struct SceSysAudioSystemThreadArgs {
  uint32_t threadId;
};

struct SceSysAudioSystemPortAndThreadArgs {
  uint32_t audioPort;
  uint32_t threadId;
};

static void createAudioSystemObjects(orbis::Process *process) {
  auto audioOut = orbis::Ref<AudioOut>(orbis::knew<AudioOut>());

  createIpmiServer(process, "SceSysAudioSystemIpc")
      .addSyncMethod<SceSysAudioSystemThreadArgs>(
          0x12340000,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemCreateControl",
                           args.threadId);
            audioOut->channelInfo.idControl = args.threadId;
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemThreadArgs>(
          0x1234000f,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemOpenMixFlag", args.threadId);
            // very bad
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "sceAudioOutMix%x",
                          args.threadId);
            auto [eventFlag, inserted] =
                orbis::g_context.createEventFlag(buffer, 0x100, 0);

            if (!inserted) {
              return 17; // FIXME: verify
            }

            audioOut->channelInfo.evf = eventFlag;
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemPortAndThreadArgs>(
          0x12340001,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemOpenPort", args.threadId,
                           args.audioPort);
            audioOut->channelInfo.port = args.audioPort;
            audioOut->channelInfo.channel = args.threadId;
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemPortAndThreadArgs>(
          0x12340002,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemStartListening",
                           args.threadId, args.audioPort);

            audioOut->start();
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemPortAndThreadArgs>(
          0x12340006, [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemStopListening",
                           args.audioPort, args.threadId);
            // TODO: implement
            return 0;
          });
}

struct SceMbusIpcAddHandleByUserIdMethodArgs {
  orbis::uint32_t unk; // 0
  orbis::uint32_t deviceId;
  orbis::uint32_t userId;
  orbis::uint32_t type;
  orbis::uint32_t index;
  orbis::uint32_t reserved;
  orbis::uint32_t pid;
};

static_assert(sizeof(SceMbusIpcAddHandleByUserIdMethodArgs) == 0x1c);

struct SceUserServiceEvent {
  std::uint32_t eventType; // 0 - login, 1 - logout
  std::uint32_t user;
};

static void createSysCoreObjects(orbis::Process *process) {
  createIpmiServer(process, "SceMbusIpc")
      .addSyncMethod<SceMbusIpcAddHandleByUserIdMethodArgs>(
          0xce110007, [](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceMbusIpcAddHandleByUserId", args.unk,
                           args.deviceId, args.userId, args.type, args.index,
                           args.reserved, args.pid);
            return 0;
          });
  createIpmiServer(process, "SceSysCoreApp");
  createIpmiServer(process, "SceSysCoreApp2");
  createIpmiServer(process, "SceMDBG0SRV");

  createSemaphore("SceSysCoreProcSpawnSema", 0x101, 0, 1);
  createSemaphore("SceTraceMemorySem", 0x100, 1, 1);
  createSemaphore("SceSysCoreEventSemaphore", 0x101, 0, 0x2d2);
  createSemaphore("SceSysCoreProcSema", 0x101, 0, 1);
  createSemaphore("AppmgrCoredumpHandlingEventSema", 0x101, 0, 4);

  createEventFlag("SceMdbgVrTriggerDump", 0x121, 0);
}

static void createGnmCompositorObjects(orbis::Process *process) {
  createEventFlag("SceCompositorCrashEventFlags", 0x122, 0);
  createEventFlag("SceCompositorEventflag", 0x122, 0);
  createEventFlag("SceCompositorResetStatusEVF", 0x122, 0);

  createShm("/tmp/SceHmd/Vr2d_shm_pass", 0xa02, 0x1b6, 16384);
}

static void createShellCoreObjects(orbis::Process *process) {

  // FIXME: replace with fmt library
  auto fmtHex = [](auto value, bool upperCase = false) {
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0');
    if (upperCase) {
      ss << std::uppercase;
    }
    ss << value;
    return std::move(ss).str();
  };

  createIpmiServer(process, "SceSystemLoggerService");
  createIpmiServer(process, "SceLoginMgrServer");
  createIpmiServer(process, "SceLncService")
      .addSyncMethod(orbis::g_context.fwSdkVersion > 0x6000000 ? 0x30013
                                                               : 0x30010,
                     [](void *out, std::uint64_t &size) -> std::int32_t {
                       struct SceLncServiceAppStatus {
                         std::uint32_t unk0;
                         std::uint32_t unk1;
                         std::uint32_t unk2;
                       };

                       if (size < sizeof(SceLncServiceAppStatus)) {
                         return -1;
                       }

                       *(SceLncServiceAppStatus *)out = {
                           .unk0 = 1,
                           .unk1 = 1,
                           .unk2 = 1,
                       };

                       size = sizeof(SceLncServiceAppStatus);
                       return 0;
                     });
  createIpmiServer(process, "SceAppMessaging");
  createIpmiServer(process, "SceShellCoreUtil");
  createIpmiServer(process, "SceNetCtl");
  createIpmiServer(process, "SceNpMgrIpc")
      .addSyncMethod(
          0,
          [=](void *out, std::uint64_t &size) -> std::int32_t {
            std::string_view result = "SceNpMgrEvf";
            if (size < result.size() + 1) {
              return 0x8002'0000 + static_cast<int>(orbis::ErrorCode::INVAL);
            }
            std::strncpy((char *)out, result.data(), result.size() + 1);
            size = result.size() + 1;
            orbis::g_context.createEventFlag(orbis::kstring(result), 0x200, 0);
            return 0;
          })
      .addSyncMethodStub(0xd);
  createIpmiServer(process, "SceNpService")
      .addSyncMethod<std::uint32_t>(0, [=](void *out, std::uint64_t &size,
                                           std::uint32_t val) { return 0; })
      .addSyncMethod(0xa0001,
                     [=](void *out, std::uint64_t &size) -> std::int32_t {
                       if (size < 1) {
                         return 0x8002'0000 +
                                static_cast<int>(orbis::ErrorCode::INVAL);
                       }
                       size = 1;
                       *reinterpret_cast<std::uint8_t *>(out) = 1;
                       return 0;
                     })
      .addSyncMethod(0xa0002,
                     [=](void *out, std::uint64_t &size) -> std::int32_t {
                       if (size < 1) {
                         return 0x8002'0000 +
                                static_cast<int>(orbis::ErrorCode::INVAL);
                       }
                       size = 1;
                       *reinterpret_cast<std::uint8_t *>(out) = 1;
                       return 0;
                     })
      .addSyncMethod<std::uint32_t, std::uint32_t>(
          0xd0000, // sceNpTpipIpcClientGetShmIndex
          [=](std::uint32_t &shmIndex, std::uint32_t appId) -> std::int32_t {
            shmIndex = 0;
            return 0;
          });

  createIpmiServer(process, "SceNpTrophyIpc")
      .addSyncMethod(2,
                     [](std::vector<std::vector<std::byte>> &out,
                        const std::vector<std::span<std::byte>> &in) {
                       if (out.size() != 1 ||
                           out[0].size() < sizeof(std::uint32_t)) {
                         return orbis::ErrorCode::INVAL;
                       }
                       out = {toBytes<std::uint32_t>(0)};
                       return orbis::ErrorCode{};
                     })
      .addAsyncMethod(0x30040,
                      [](orbis::IpmiSession &session,
                         std::vector<std::vector<std::byte>> &out,
                         const std::vector<std::span<std::byte>> &in) {
                        session.client->eventFlags[0].set(1);
                        return orbis::ErrorCode{};
                      })
      .addSyncMethod(0x90000,
                     [](std::vector<std::vector<std::byte>> &out,
                        const std::vector<std::span<std::byte>> &in) {
                       if (out.size() != 1 ||
                           out[0].size() < sizeof(std::uint32_t)) {
                         return orbis::ErrorCode::INVAL;
                       }
                       out = {toBytes<std::uint32_t>(1)};
                       return orbis::ErrorCode{};
                     })
      .addSyncMethod(0x90003,
                     [](std::vector<std::vector<std::byte>> &out,
                        const std::vector<std::span<std::byte>> &in) {
                       if (out.size() != 1 ||
                           out[0].size() < sizeof(std::uint32_t)) {
                         return orbis::ErrorCode::INVAL;
                       }
                       out = {toBytes<std::uint32_t>(1)};
                       return orbis::ErrorCode{};
                     })
      .addAsyncMethod(0x90024,
                      [](orbis::IpmiSession &session,
                         std::vector<std::vector<std::byte>> &out,
                         const std::vector<std::span<std::byte>> &in) {
                        out.push_back(toBytes<std::uint32_t>(0));
                        // session.client->eventFlags[0].set(1);
                        return orbis::ErrorCode{};
                      })
      .addAsyncMethod(0x90026, [](orbis::IpmiSession &session,
                                  std::vector<std::vector<std::byte>> &out,
                                  const std::vector<std::span<std::byte>> &in) {
        session.client->eventFlags[0].set(1);
        return orbis::ErrorCode{};
      });
  createIpmiServer(process, "SceNpUdsIpc");
  createIpmiServer(process, "SceLibNpRifMgrIpc");
  createIpmiServer(process, "SceNpPartner001");
  createIpmiServer(process, "SceNpPartnerSubs");
  createIpmiServer(process, "SceNpGameIntent");
  createIpmiServer(process, "SceBgft");
  createIpmiServer(process, "SceCntMgrService");
  createIpmiServer(process, "ScePlayGo");
  createIpmiServer(process, "SceCompAppProxyUtil");
  createIpmiServer(process, "SceShareSpIpcService");
  createIpmiServer(process, "SceRnpsAppMgr");
  createIpmiServer(process, "SceUpdateService");
  createIpmiServer(process, "ScePatchChecker");
  createIpmiServer(process, "SceMorpheusUpdService");
  createIpmiServer(process, "ScePsmSharedDmem");
  createIpmiServer(process, "SceSaveData")
      .addSyncMethod(
          0x12340001,
          [](void *out, std::uint64_t &size) -> std::int32_t {
            {
              auto [dev, devPath] = rx::vfs::get("/app0");
              if (auto hostFs = dev.cast<HostFsDevice>()) {
                std::error_code ec;
                auto saveDir = hostFs->hostPath + "/.rpcsx/saves/";
                if (!std::filesystem::exists(saveDir)) {
                  return 0x8002'0000 +
                         static_cast<int>(orbis::ErrorCode::NOENT);
                }
              }
            }
            std::string_view result = "/saves";
            if (size < result.size() + 1) {
              return 0x8002'0000 + static_cast<int>(orbis::ErrorCode::INVAL);
            }
            std::strncpy((char *)out, result.data(), result.size() + 1);
            size = result.size() + 1;
            orbis::g_context.createEventFlag(orbis::kstring(result), 0x200, 0);
            return 0;
          })
      .addSyncMethod(
          0x12340002, [](void *out, std::uint64_t &size) -> std::int32_t {
            {
              auto [dev, devPath] = rx::vfs::get("/app0");
              if (auto hostFs = dev.cast<HostFsDevice>()) {
                std::error_code ec;
                auto saveDir = hostFs->hostPath + "/.rpcsx/saves/";
                std::filesystem::create_directories(saveDir, ec);
                rx::vfs::mount("/saves/",
                               createHostIoDevice(saveDir, "/saves/"));
              }
            }
            return 0;
          });

  createIpmiServer(process, "SceStickerCoreServer");
  createIpmiServer(process, "SceDbRecoveryShellCore");
  createIpmiServer(process, "SceUserService")
      .sendMsg(SceUserServiceEvent{.eventType = 0, .user = 1})
      .addSyncMethod(0x30011,
                     [](void *ptr, std::uint64_t &size) -> std::int32_t {
                       if (size < sizeof(orbis::uint32_t)) {
                         return 0x8000'0000;
                       }

                       *(orbis::uint32_t *)ptr = 1;
                       size = sizeof(orbis::uint32_t);
                       return 0;
                     });

  createIpmiServer(process, "SceDbPreparationServer");
  createIpmiServer(process, "SceScreenShot");
  createIpmiServer(process, "SceAppDbIpc");
  createIpmiServer(process, "SceAppInst");
  createIpmiServer(process, "SceAppContent");
  createIpmiServer(process, "SceNpEntAccess");
  createIpmiServer(process, "SceMwIPMIServer");
  createIpmiServer(process, "SceAutoMounterIpc");
  createIpmiServer(process, "SceBackupRestoreUtil");
  createIpmiServer(process, "SceDataTransfer");
  createIpmiServer(process, "SceEventService");
  createIpmiServer(process, "SceShareFactoryUtil");
  createIpmiServer(process, "SceCloudConnectManager");
  createIpmiServer(process, "SceHubAppUtil");
  createIpmiServer(process, "SceTcIPMIServer");

  createSemaphore("SceLncSuspendBlock00000001", 0x101, 1, 1);
  createSemaphore("SceAppMessaging00000001", 0x100, 1, 0x7fffffff);

  createEventFlag("SceAutoMountUsbMass", 0x120, 0);
  createEventFlag("SceLoginMgrUtilityEventFlag", 0x112, 0);
  createEventFlag("SceLoginMgrSharePlayEventFlag", 0x112, 0);
  createEventFlag("SceLoginMgrServerHmdConnect", 0x112, 0);
  createEventFlag("SceLoginMgrServerDialogRequest", 0x112, 0);
  createEventFlag("SceLoginMgrServerDialogResponse", 0x112, 0);
  createEventFlag("SceGameLiveStreamingSpectator", 0x120, 0x8000000000000000);
  createEventFlag("SceGameLiveStreamingUserId", 0x120, 0x8000000000000000);
  createEventFlag("SceGameLiveStreamingMsgCount", 0x120, 0x8000000000000000);
  createEventFlag("SceGameLiveStreamingBCCtrl", 0x120, 0);
  createEventFlag("SceGameLiveStreamingEvntArg", 0x120, 0);
  createEventFlag("SceLncUtilSystemStatus", 0x120, 0);
  createEventFlag("SceShellCoreUtilRunLevel", 0x100, 0);
  createEventFlag("SceSystemStateMgrInfo", 0x120, 0x10000000a);
  createEventFlag("SceSystemStateMgrStatus", 0x120, 0);
  createEventFlag("SceAppInstallerEventFlag", 0x120, 0);
  createEventFlag("SceShellCoreUtilPowerControl", 0x120, 0x400000);
  createEventFlag("SceShellCoreUtilAppFocus", 0x120, 1);
  createEventFlag("SceShellCoreUtilCtrlFocus", 0x120, 0);
  createEventFlag("SceShellCoreUtilUIStatus", 0x120, 0x20001);
  createEventFlag("SceShellCoreUtilDevIdxBehavior", 0x120, 0);
  createEventFlag("SceNpMgrVshReq", 0x121, 0);
  createEventFlag("SceNpIdMapperVshReq", 0x121, 0);
  createEventFlag("SceRtcUtilTzdataUpdateFlag", 0x120, 0);
  createEventFlag("SceDataTransfer", 0x120, 0);

  createEventFlag("SceLncUtilAppStatus1", 0x100, 0);
  createEventFlag("SceAppMessaging1", 0x120, 1);
  createEventFlag("SceShellCoreUtil1", 0x120, 0x3f8c);
  createEventFlag("SceNpScoreIpc_" + fmtHex(process->pid), 0x120, 0);
  createEventFlag("/vmicDdEvfAin", 0x120, 0);

  createSemaphore("SceAppMessaging1", 0x101, 1, 0x7fffffff);
  createSemaphore("SceLncSuspendBlock1", 0x101, 1, 10000);

  createShm("SceGlsSharedMemory", 0x202, 0x1a4, 262144);
  createShm("SceShellCoreUtil", 0x202, 0x1a4, 16384);
  createShm("SceNpTpip", 0x202, 0x1ff, 43008);

  createShm("vmicDdShmAin", 0x202, 0x1b6, 43008);

  createSemaphore("SceNpTpip 0", 0x101, 0, 1);
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

template <typename T = std::byte> struct GuestAlloc {
  orbis::ptr<T> guestAddress;

  GuestAlloc(std::size_t size) {
    if (size == 0) {
      guestAddress = nullptr;
    } else {
      guestAddress = orbis::ptr<T>(rx::vm::map(
          nullptr, size, rx::vm::kMapProtCpuRead | rx::vm::kMapProtCpuWrite,
          rx::vm::kMapFlagPrivate | rx::vm::kMapFlagAnonymous));
    }
  }

  GuestAlloc() : GuestAlloc(sizeof(T)) {}

  GuestAlloc(const T &data) : GuestAlloc() {
    if (orbis::uwrite(guestAddress, data) != orbis::ErrorCode{}) {
      std::abort();
    }
  }

  GuestAlloc(const void *data, std::size_t size) : GuestAlloc(size) {
    if (orbis::uwriteRaw(guestAddress, data, size) != orbis::ErrorCode{}) {
      std::abort();
    }
  }

  GuestAlloc(const GuestAlloc &) = delete;

  GuestAlloc(GuestAlloc &&other) : guestAddress(other.guestAddress) {
    other.guestAddress = 0;
  }
  GuestAlloc &operator=(GuestAlloc &&other) {
    std::swap(guestAddress, other.guestAddress);
  }

  ~GuestAlloc() {
    if (guestAddress != 0) {
      rx::vm::unmap(guestAddress, sizeof(T));
    }
  }

  operator orbis::ptr<T>() { return guestAddress; }
  T *operator->() { return guestAddress; }
  operator T &() { return *guestAddress; }
};

struct IpmiClient {
  orbis::Ref<orbis::IpmiClient> clientImpl;
  orbis::uint kid;
  orbis::Thread *thread;

  orbis::sint
  sendSyncMessageRaw(std::uint32_t method,
                     const std::vector<std::vector<std::byte>> &inData,
                     std::vector<std::vector<std::byte>> &outBuf) {
    GuestAlloc<orbis::sint> serverResult;
    GuestAlloc<orbis::IpmiDataInfo> guestInDataArray{
        sizeof(orbis::IpmiDataInfo) * inData.size()};
    GuestAlloc<orbis::IpmiBufferInfo> guestOutBufArray{
        sizeof(orbis::IpmiBufferInfo) * outBuf.size()};

    std::vector<GuestAlloc<std::byte>> guestAllocs;
    guestAllocs.reserve(inData.size() + outBuf.size());

    for (auto &data : inData) {
      auto pointer =
          guestAllocs.emplace_back(data.data(), data.size()).guestAddress;

      guestInDataArray.guestAddress[&data - inData.data()] = {
          .data = pointer, .size = data.size()};
    }

    for (auto &buf : outBuf) {
      auto pointer =
          guestAllocs.emplace_back(buf.data(), buf.size()).guestAddress;

      guestOutBufArray.guestAddress[&buf - outBuf.data()] = {
          .data = pointer, .capacity = buf.size()};
    }

    GuestAlloc params = orbis::IpmiSyncCallParams{
        .method = method,
        .numInData = static_cast<orbis::uint32_t>(inData.size()),
        .numOutData = static_cast<orbis::uint32_t>(outBuf.size()),
        .pInData = guestInDataArray,
        .pOutData = guestOutBufArray,
        .pResult = serverResult,
    };

    GuestAlloc<orbis::uint> errorCode;
    orbis::sysIpmiClientInvokeSyncMethod(thread, errorCode, kid, params,
                                         sizeof(orbis::IpmiSyncCallParams));

    for (auto &buf : outBuf) {
      auto size = guestOutBufArray.guestAddress[inData.data() - &buf].size;
      buf.resize(size);
    }
    return serverResult;
  }

  template <typename... InputTypes>
  orbis::sint sendSyncMessage(std::uint32_t method,
                              const InputTypes &...input) {
    std::vector<std::vector<std::byte>> outBuf;
    return sendSyncMessageRaw(method, {toBytes(input)...}, outBuf);
  }

  template <typename... OutputTypes, typename... InputTypes>
    requires((sizeof...(OutputTypes) > 0) || sizeof...(InputTypes) == 0)
  std::tuple<OutputTypes...> sendSyncMessage(std::uint32_t method,
                                             InputTypes... input) {
    std::vector<std::vector<std::byte>> outBuf{sizeof(OutputTypes)...};
    sendSyncMessageRaw(method, {toBytes(input)...}, outBuf);
    std::tuple<OutputTypes...> output;

    auto unpack = [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((std::get<I>(output) = *reinterpret_cast<OutputTypes *>(outBuf.data())),
       ...);
    };
    unpack(std::make_index_sequence<sizeof...(OutputTypes)>{});
    return output;
  }
};

static IpmiClient createIpmiClient(orbis::Thread *thread, const char *name) {
  orbis::Ref<orbis::IpmiClient> client;
  GuestAlloc config = orbis::IpmiCreateClientConfig{
      .size = sizeof(orbis::IpmiCreateClientConfig),
  };

  orbis::uint kid;

  {
    GuestAlloc<char> guestName{name, std::strlen(name)};
    GuestAlloc params = orbis::IpmiCreateClientParams{
        .name = guestName,
        .config = config,
    };

    GuestAlloc<orbis::uint> result;
    GuestAlloc<orbis::uint> guestKid;
    orbis::sysIpmiCreateClient(thread, guestKid, params,
                               sizeof(orbis::IpmiCreateClientParams));
    kid = guestKid;
  }

  {
    GuestAlloc<orbis::sint> status;
    GuestAlloc params = orbis::IpmiClientConnectParams{.status = status};

    GuestAlloc<orbis::uint> result;
    orbis::sysIpmiClientConnect(thread, result, kid, params,
                                sizeof(orbis::IpmiClientConnectParams));
  }

  return {std::move(client), kid, thread};
}

static orbis::SysResult launchDaemon(orbis::Thread *thread, std::string path,
                                     std::vector<std::string> argv,
                                     std::vector<std::string> envv) {
  auto childPid = orbis::g_context.allocatePid() * 10000 + 1;
  auto flag = orbis::knew<std::atomic<bool>>();
  *flag = false;

  int hostPid = ::fork();

  if (hostPid) {
    while (*flag == false) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    orbis::kfree(flag, sizeof(*flag));
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
  process->appInfo = {
      .unk4 = orbis::slong(0x80000000'00000000),
  };

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

  rx::vm::fork(childPid);
  rx::vfs::fork();

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
    auto result = rx::vfs::open(path, kOpenFlagReadOnly, 0, &file, thread);
    if (result.isError()) {
      return result;
    }
  }

  rx::vm::reset();

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

  setupSigHandlers();
  rx::vfs::initialize();

  bool enableAudio = false;
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

      std::printf("mounting '%s' to virtual '%s'\n", argv[argIndex + 1],
                  argv[argIndex + 2]);
      if (!std::filesystem::is_directory(argv[argIndex + 1])) {
        std::fprintf(stderr, "Directory '%s' not exists\n", argv[argIndex + 1]);
        return 1;
      }

      rx::vfs::mount(
          argv[argIndex + 2],
          createHostIoDevice(argv[argIndex + 1], argv[argIndex + 2]));
      argIndex += 3;
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
      enableAudio = true;
      continue;
    }

    break;
  }

  if (argIndex >= argc) {
    usage(argv[0]);
    return 1;
  }

  rx::thread::initialize();

  // rx::vm::printHostStats();
  orbis::g_context.allocatePid();
  auto initProcess = orbis::g_context.createProcess(asRoot ? 1 : 10);
  // pthread_setname_np(pthread_self(), "10.MAINTHREAD");

  rx::vm::initialize(initProcess->pid);
  runRpcsxGpu();

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

  auto executableModule =
      rx::linker::loadModuleFile(argv[argIndex], mainThread);

  if (executableModule == nullptr) {
    std::fprintf(stderr, "Failed to open '%s'\n", argv[argIndex]);
    std::abort();
  }

  executableModule->id = initProcess->modulesMap.insert(executableModule);
  initProcess->processParam = executableModule->processParam;
  initProcess->processParamSize = executableModule->processParamSize;

  if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
            (void *)0x100'0000'0000, ~0ull - 0x100'0000'0000, nullptr)) {
    perror("prctl failed\n");
    exit(-1);
  }

  if (executableModule->type == rx::linker::kElfTypeSceDynExec ||
      executableModule->type == rx::linker::kElfTypeSceExec ||
      executableModule->type == rx::linker::kElfTypeExec) {
    ps4InitDev();
    ps4InitFd(mainThread);

    std::vector<std::string> ps4Argv(argv + argIndex,
                                     argv + argIndex + argc - argIndex);

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
      createMiniSysCoreObjects(initProcess);
      createSysAvControlObjects(initProcess);
      createSysCoreObjects(initProcess);
      createGnmCompositorObjects(initProcess);
      createShellCoreObjects(initProcess);
      if (enableAudio) {
        createAudioSystemObjects(initProcess);
      }

      // ?
      createIpmiServer(initProcess, "SceCdlgRichProf");
      createIpmiServer(initProcess, "SceRemoteplayIpc");
      createIpmiServer(initProcess, "SceGlsIpc");
      createIpmiServer(initProcess, "SceImeService");
      createIpmiServer(initProcess, "SceErrorDlgServ");

      createEventFlag("SceNpTusIpc_0000000a", 0x120, 0);
      createSemaphore("SceLncSuspendBlock00000000", 0x101, 1, 1);
      createSemaphore("SceNpPlusLogger 0", 0x101, 0, 0x7fffffff);

      createSemaphore("SceSaveData0000000000000001", 0x101, 0, 1);
      createSemaphore("SceSaveData0000000000000001_0", 0x101, 0, 1);
      createShm("SceSaveData0000000000000001_0", 0x202, 0x1b6, 0x40000);
      createShm("SceSaveDataI0000000000000001", 0x202, 0x1b6, 43008);
      createShm("SceSaveDataI0000000000000001_0", 0x202, 0x1b6, 43008);
      createShm("SceNpPlusLogger", 0x202, 0x1b6, 0x40000);
      createEventFlag("SceSaveDataMemoryRUI00000010", 0x120, 1);
      initProcess->cwd = "/app0/";

      if (!enableAudio) {
        launchDaemon(mainThread, "/system/sys/orbis_audiod.elf",
                     {"/system/sys/orbis_audiod.elf"}, {});
      }
      status = ps4Exec(mainThread, execEnv, std::move(executableModule),
                       ps4Argv, {});
    }
    status =
        ps4Exec(mainThread, execEnv, std::move(executableModule), ps4Argv, {});
  } else {
    std::fprintf(stderr, "Unexpected executable type\n");
    status = 1;
  }

  // rx::vm::printHostStats();
  rx::vm::deinitialize();
  rx::thread::deinitialize();

  return status;
}
