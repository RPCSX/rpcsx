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
#include "orbis/dmem.hpp"
#include "orbis/fmem.hpp"
#include "orbis/pmem.hpp"
#include "orbis/ucontext.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/vmem.hpp"
#include "rx/Config.hpp"
#include "rx/FileLock.hpp"
#include "rx/Mappable.hpp"
#include "rx/die.hpp"
#include "rx/format.hpp"
#include "rx/mem.hpp"
#include "rx/print.hpp"
#include "rx/watchdog.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vk.hpp"
#include "xbyak/xbyak.h"
#include <bit>
#include <optional>
#include <rx/Rc.hpp>
#include <rx/Version.hpp>
#include <rx/align.hpp>
#include <rx/hexdump.hpp>

#include <elf.h>
#include <linux/prctl.h>

#include <orbis/KernelAllocator.hpp>
#include <orbis/KernelContext.hpp>
#include <orbis/KernelObject.hpp>
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
#include <thread>
#include <ucontext.h>

#include <atomic>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>

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
    auto process = orbis::g_currentThread->tproc;
    auto vmid = process->vmId;
    auto ctx = reinterpret_cast<ucontext_t *>(ucontext);
    bool isWrite = (ctx->uc_mcontext.gregs[REG_ERR] & 0x2) != 0;
    auto origVmProt = orbis::vmem::queryProtection(process, signalAddress);

    auto page = signalAddress / rx::mem::pageSize;
    auto gpuDevice = amdgpu::DeviceCtl{orbis::g_context->gpuDevice};

    if (gpuDevice && origVmProt &&
        (origVmProt->prot & (isWrite ? orbis::vmem::Protection::CpuWrite
                                     : orbis::vmem::Protection::CpuRead))) {
      auto prot = toCpuProtection(origVmProt->prot);
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
          prot = prot & ~rx::mem::Protection::W;
          break;
        }

        if (gpuContext.cachePages[vmid][page].compare_exchange_weak(
                flags, amdgpu::kPageInvalidated, std::memory_order::relaxed)) {
          break;
        }
      }

      auto range = rx::AddressRange::fromBeginSize(page * rx::mem::pageSize,
                                                   rx::mem::pageSize);

      auto errc = rx::mem::protect(range, prot);
      rx::dieIf(errc != std::errc{},
                "cache: virtual memory protection failed, address {}, error {}",
                range.beginAddress(), static_cast<int>(errc));
      _writefsbase_u64(orbis::g_currentThread->fsBase);
      return;
    }

    std::fprintf(stderr, "SIGSEGV, address %lx, access %s, prot %u\n",
                 signalAddress, isWrite ? "write" : "read",
                 origVmProt ? origVmProt->prot.toUnderlying() : -1);
  }

  if (orbis::g_currentThread != nullptr) {
    orbis::g_currentThread->tproc->exitStatus = sig;
    orbis::g_currentThread->tproc->event.emit(orbis::kEvFiltProc,
                                              orbis::kNoteExit, sig);
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

    if (orbis::g_currentThread != nullptr) {
      auto toGuestSigno = [](int sig) -> std::optional<orbis::Signal> {
        switch (sig) {
        case SIGSEGV:
          return orbis::kSigSegv;

        case SIGBUS:
          return orbis::kSigBus;

        case SIGFPE:
          return orbis::kSigFpe;

        default:
          return std::nullopt;
        }
      };

      if (auto guestSigno = toGuestSigno(sig)) {
        auto context = reinterpret_cast<ucontext_t *>(ucontext);
        bool inGuestCode =
            context->uc_mcontext.gregs[REG_RIP] < orbis::kMaxAddress;

        if (inGuestCode) {
          if (rx::thread::invokeSignalHandler(orbis::g_currentThread, info,
                                              *guestSigno, context)) {
            return;
          }
        }
      }
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
static const orbis::sysent *getSyscallEnt(orbis::Thread *thread, int sysno) {
  auto sysvec = thread->tproc->sysent;

  if (sysno >= sysvec->size) {
    return nullptr;
  }

  return sysvec->table + sysno;
}
static void onSysEnter(orbis::Thread *thread, int id, uint64_t *args,
                       int argsCount) {
  if (!g_traceSyscalls) {
    return;
  }

  rx::ScopedFileLock lock(stderr);
  rx::print(stderr, "   [{}] ", thread->tid);

  if (auto ent = getSyscallEnt(thread, id)) {
    rx::println(stderr, "{}", ent->format(thread, args).c_str());
    return;
  }

  rx::print(stderr, "sys_{}(", id);

  for (int i = 0; i < argsCount; ++i) {
    if (i != 0) {
      rx::print(stderr, ", ");
    }

    rx::print(stderr, "{:#x}", args[i]);
  }

  rx::println(stderr, ")");
}

static void onSysExit(orbis::Thread *thread, int id, uint64_t *args,
                      int argsCount, orbis::SysResult result) {
  if (!result.isError() && !g_traceSyscalls) {
    return;
  }

  rx::ScopedFileLock lock(stderr);
  rx::print(stderr, "{}: [{}] ", result.isError() ? 'E' : 'S', thread->tid);

  if (auto ent = getSyscallEnt(thread, id)) {
    rx::print(stderr, "{}", ent->format(thread, args));
  } else {
    rx::print(stderr, "sys_{}(", id);

    for (int i = 0; i < argsCount; ++i) {
      if (i != 0) {
        rx::print(stderr, ", ");
      }

      rx::print(stderr, "{:#x}", args[i]);
    }

    rx::print(stderr, ")");
  }

  rx::println(stderr, " -> {}, Value {:x}:{:x}", result.errc(),
              thread->retval[0], thread->retval[1]);

  if (result.isError()) {
    thread->where();
  }
}

static void guestInitDev(orbis::Thread *thread, int stdinFd, int stdoutFd,
                         int stderrFd) {
  auto dmem0 = createDmemCharacterDevice(0);
  dmem0->open(&orbis::g_context->dmem, "", 0, 0, thread);

  auto dce = createDceCharacterDevice();
  orbis::g_context->dceDevice = dce;

  auto consoleDev = createConsoleCharacterDevice(stdinFd, stdoutFd);
  auto mbus = static_cast<MBusDevice *>(createMBusCharacterDevice());
  auto mbusAv = static_cast<MBusAVDevice *>(createMBusAVCharacterDevice());

  // FIXME: make it configurable
  auto defaultAudioDevice = orbis::knew<AlsaDevice>();
  auto nullAudioDevice = orbis::knew<AudioDevice>();

  auto hdmiAudioDevice = defaultAudioDevice;
  auto analogAudioDevice = nullAudioDevice;
  auto spdifAudioDevice = nullAudioDevice;

  vfs::addDevice("npdrm", createNpdrmCharacterDevice());
  vfs::addDevice("icc_configuration", createIccConfigurationCharacterDevice());
  vfs::addDevice("console", consoleDev);
  vfs::addDevice("camera", createCameraCharacterDevice());
  vfs::addDevice("dmem0", dmem0);
  vfs::addDevice("dmem1", createDmemCharacterDevice(1));
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
  vfs::addDevice("evlg1", createEvlgCharacterDevice(stderrFd));
  vfs::addDevice("srtc", createSrtcCharacterDevice());
  vfs::addDevice("sshot", createScreenShotCharacterDevice());
  vfs::addDevice("lvdctl", createLvdCtlCharacterDevice());
  vfs::addDevice("lvd0", createHddCharacterDevice(0x100000000));
  vfs::addDevice("icc_power", createIccPowerCharacterDevice());
  vfs::addDevice("cayman/reg", createCaymanRegCharacterDevice());
  vfs::addDevice("hctrl", createHidCharacterDevice());

  if (orbis::g_context->fwType == orbis::FwType::Ps5) {
    vfs::addDevice("iccnvs4", createIccPowerCharacterDevice());
    vfs::addDevice("ajmi", createAjmCharacterDevice());
    vfs::addDevice("ssd0", createHddCharacterDevice(0x100000000));
    vfs::addDevice("nsid1.ctl", createNsidCtlCharacterDevice());
    vfs::addDevice("ssd0.swapx2", createHddCharacterDevice(0x100000000));
    vfs::addDevice("ssd0.bd_rvlist", createHddCharacterDevice(1048576));
    vfs::addDevice("ssd0.system", createHddCharacterDevice(671088640));
    vfs::addDevice("ssd0.system_ex", createHddCharacterDevice(1610612736));
    vfs::addDevice("ssd0.system_b", createHddCharacterDevice(671088640));
    vfs::addDevice("ssd0.system_ex_b", createHddCharacterDevice(1610612736));
    vfs::addDevice("ssd0.preinst", createHddCharacterDevice(155189248));
    vfs::addDevice("ssd0.app_temp0", createHddCharacterDevice(1073741824));
    // vfs::addDevice("ssd0.app_temp1", createHddCharacterDevice(1073741824));
    vfs::addDevice("ssd0.system_data", createHddCharacterDevice(8589934592));
    vfs::addDevice("ssd0.update", createHddCharacterDevice(4294967296));
    vfs::addDevice("ssd0.swap", createHddCharacterDevice(8589934592));
    vfs::addDevice("ssd0.app_swap", createHddCharacterDevice(15032385536));
    vfs::addDevice("ssd0.hibernation", createHddCharacterDevice(3623878656));
    vfs::addDevice("ssd0.user",
                   createHddCharacterDevice(-41630302208)); /// ?????
    vfs::addDevice("ssd0.user_bfs", createHddCharacterDevice(0x100000000));
    vfs::addDevice("bfs/ctl", createHddCharacterDevice(0x100000000));
    vfs::addDevice("md2", createHddCharacterDevice(0x100000000));
    vfs::addDevice("a53io", createA53IoCharacterDevice());

    vfs::addDevice("hmd2_cmd", createHmd2CmdCharacterDevice());
    vfs::addDevice("hmd2_imu", createHmd2ImuCharacterDevice());
    vfs::addDevice("hmd2_gaze", createHmd2GazeCharacterDevice());
    vfs::addDevice("hmd2_gen_data", createHmd2GenDataCharacterDevice());
  }

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
  orbis::g_context->shmDevice = shm;
  createBlockPoolDevice()->open(&orbis::g_context->blockpool, "", 0, 0, thread);
}

static void guestInitFd(orbis::Thread *mainThread) {
  rx::Ref<orbis::File> stdinFile;
  rx::Ref<orbis::File> stdoutFile;
  rx::Ref<orbis::File> stderrFile;
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

int guestExec(orbis::Thread *mainThread, ExecEnv execEnv,
              rx::Ref<orbis::Module> executableModule,
              std::span<std::string> argv, std::span<std::string> envp) {
  const auto stackEndAddress = 0x7'eeff'c000ull;
  const auto stackSize = 0x200000;

  auto stackStartAddress = stackEndAddress - stackSize;

  auto [stackVmRange, vmErrc] = orbis::vmem::mapFlex(
      mainThread->tproc, stackSize,
      orbis::vmem::Protection::CpuRead | orbis::vmem::Protection::CpuWrite,
      stackStartAddress,
      orbis::AllocationFlags::Stack | orbis::AllocationFlags::Fixed,
      orbis::vmem::BlockFlags::Stack, "main stack");

  rx::dieIf(vmErrc != orbis::ErrorCode{},
            "failed to map main thread stack, error {}",
            static_cast<int>(vmErrc));

  mainThread->stackStart = stackVmRange.beginAddress();
  mainThread->stackEnd = stackVmRange.endAddress();

  std::vector<std::uint64_t> argvOffsets;
  std::vector<std::uint64_t> envpOffsets;

  StackWriter stack{mainThread->stackEnd};

  for (auto &elem : argv) {
    argvOffsets.push_back(stack.pushString(elem.data()));
  }

  argvOffsets.push_back(0);

  for (auto &elem : envp) {
    envpOffsets.push_back(stack.pushString(elem.data()));
  }

  envpOffsets.push_back(0);

  if (executableModule->dynType == orbis::DynType::Ps4) {
    mainThread->tproc->type = orbis::ProcessType::Ps4;
    mainThread->tproc->sysent = &orbis::ps4_sysvec;
  } else if (executableModule->dynType == orbis::DynType::Ps5) {
    mainThread->tproc->type = orbis::ProcessType::Ps5;
    mainThread->tproc->sysent = &orbis::ps5_sysvec;
  } else {
    if (orbis::g_context->fwType == orbis::FwType::Ps4) {
      mainThread->tproc->sysent = &orbis::ps4_sysvec;
    } else {
      mainThread->tproc->sysent = &orbis::ps5_sysvec;
    }
  }

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

struct ProcessParam {
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

ExecEnv guestCreateExecEnv(orbis::Thread *mainThread,
                           const rx::Ref<orbis::Module> &executableModule,
                           bool isSystem) {
  std::uint64_t interpBase = 0;
  std::uint64_t entryPoint = executableModule->entryPoint;

  if (mainThread->tproc->processParam != nullptr &&
      mainThread->tproc->processParamSize >= sizeof(ProcessParam)) {
    auto processParam =
        reinterpret_cast<ProcessParam *>(mainThread->tproc->processParam);
    mainThread->tproc->sdkVersion = processParam->sdkVersion;
  }

  if (orbis::g_context->sdkVersion == 0 && mainThread->tproc->sdkVersion != 0) {
    orbis::g_context->sdkVersion = mainThread->tproc->sdkVersion;
  }
  if (mainThread->tproc->sdkVersion == 0) {
    mainThread->tproc->sdkVersion = orbis::g_context->sdkVersion;
  }

  if (executableModule->dynType == orbis::DynType::None) {
    return {.entryPoint = entryPoint, .interpBase = interpBase};
  }

  if (!executableModule->interp.empty() &&
      vfs::exists(executableModule->interp, mainThread)) {
    auto loader =
        rx::linker::loadModuleFile(executableModule->interp, mainThread);
    loader->id = mainThread->tproc->modulesMap.insert(loader);
    interpBase = reinterpret_cast<std::uint64_t>(loader->base);
    entryPoint = loader->entryPoint;

    return {.entryPoint = entryPoint, .interpBase = interpBase};
  }

  auto libkernel = rx::linker::loadModuleFile(
      (isSystem ? "/system/common/lib/libkernel_sys.sprx"
                : "/system/common/lib/libkernel.sprx"),
      mainThread);

  rx::dieIf(libkernel == nullptr, "libkernel not found");
  libkernel->id = mainThread->tproc->modulesMap.insert(libkernel);

  mainThread->tproc->libkernelRange = rx::AddressRange::fromBeginSize(
      std::bit_cast<orbis::uintptr_t>(libkernel->base), libkernel->size);

  auto libSceLibcInternal = rx::linker::loadModuleFile(
      "/system/common/lib/libSceLibcInternal.sprx", mainThread);

  rx::dieIf(libSceLibcInternal == nullptr, "libSceLibcInternal not found");

  libSceLibcInternal->id =
      mainThread->tproc->modulesMap.insert(libSceLibcInternal);

  if (orbis::g_context->fwType == orbis::FwType::Ps4) {
    for (auto sym : libkernel->symbols) {
      if (sym.id == 0xd2f4e7e480cc53d0) {
        auto address = (uint64_t)libkernel->base + sym.address;
        ::mprotect((void *)rx::alignDown(address, 0x1000),
                   rx::alignUp(sym.size + sym.address, 0x1000), PROT_WRITE);
        rx::println("patching sceKernelGetMainSocId");
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
  }

  if (orbis::g_context->fwSdkVersion == 0) {
    auto moduleParam = reinterpret_cast<std::byte *>(libkernel->moduleParam);
    auto fwSdkVersion = moduleParam         //
                        + sizeof(uint64_t)  // size
                        + sizeof(uint64_t); // magic
    orbis::g_context->fwSdkVersion = *(uint32_t *)fwSdkVersion;
    std::printf("fw sdk version: %x\n", orbis::g_context->fwSdkVersion);
  }

  if (orbis::g_context->fwType == orbis::FwType::Unknown) {
    if (libkernel->dynType == orbis::DynType::Ps4) {
      orbis::g_context->fwType = orbis::FwType::Ps4;
    } else {
      orbis::g_context->fwType = orbis::FwType::Ps5;
    }
  }

  interpBase = reinterpret_cast<std::uint64_t>(libkernel->base);
  entryPoint = libkernel->entryPoint;

  return {.entryPoint = entryPoint, .interpBase = interpBase};
}

int guestExec(orbis::Thread *mainThread,
              rx::Ref<orbis::Module> executableModule,
              std::span<std::string> argv, std::span<std::string> envp) {
  auto execEnv = guestCreateExecEnv(mainThread, executableModule, true);
  return guestExec(mainThread, execEnv, std::move(executableModule), argv,
                   envp);
}

static void usage(const char *argv0) {
  rx::println("{} [<options>...] <virtual path to elf> [args...]", argv0);
  rx::println("  options:");
  rx::println("  --version, -v - print version");
  rx::println("    -m, --mount <host path> <virtual path>");
  rx::println("    -o, --override <original module name> <virtual path to "
              "overriden module>");
  rx::println("    --fw <path to firmware root>");
  rx::println(
      "    --gpu <index> - specify physical gpu index to use, default is 0");
  rx::println("    --disable-cache - disable cache of gpu resources");
  // rx::println("    --presenter <window>");
  rx::println("    --trace");
}

static orbis::SysResult launchDaemon(orbis::Thread *thread, std::string path,
                                     std::vector<std::string> argv,
                                     std::vector<std::string> envv,
                                     orbis::AppInfoEx appInfo) {
  auto childPid = orbis::allocatePid();
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

  auto process = orbis::createProcess(thread->tproc, childPid);
  orbis::vmem::initialize(process, true); // override init process mappings
  auto logFd = ::open(("log-" + std::to_string(childPid) + ".txt").c_str(),
                      O_CREAT | O_TRUNC | O_WRONLY, 0666);

  dup2(logFd, 1);
  dup2(logFd, 2);

  process->hostPid = ::getpid();
  process->sysent = thread->tproc->sysent;
  process->onSysEnter = thread->tproc->onSysEnter;
  process->onSysExit = thread->tproc->onSysExit;
  process->ops = thread->tproc->ops;
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
  process->budgetId = thread->tproc->budgetId;
  process->isInSandbox = false;

  vfs::fork();

  *flag = true;

  auto newThread = orbis::createThread(process, path);
  newThread->hostTid = ::gettid();
  newThread->nativeHandle = pthread_self();
  newThread->context = thread->context;
  newThread->fsBase = thread->fsBase;

  orbis::g_currentThread = newThread;
  thread = orbis::g_currentThread;

  setupSigHandlers();
  rx::thread::initialize();
  rx::thread::setupThisThread();

  guestInitFd(newThread);

  rx::Ref<orbis::File> socket;
  createSocket(&socket, "", 1, 1, 0);
  process->fileDescriptors.insert(socket);

  ORBIS_LOG_ERROR(__FUNCTION__, path);

  {
    rx::Ref<orbis::File> file;
    auto result = vfs::open(path, orbis::kOpenFlagReadOnly, 0, &file, thread);
    if (result.isError()) {
      return result;
    }
  }

  thread->tproc->nextTlsSlot = 1;
  auto executableModule = rx::linker::loadModuleFile(path, thread);

  executableModule->id = thread->tproc->modulesMap.insert(executableModule);
  thread->tproc->processParam = executableModule->processParam;
  thread->tproc->processParamSize = executableModule->processParamSize;

  g_traceSyscalls = false;

  thread->tproc->event.emit(orbis::kEvFiltProc, orbis::kNoteExec);

  thread->handle = std::thread([&] {
    thread->hostTid = ::gettid();
    rx::thread::initialize();
    rx::thread::setupSignalStack();
    rx::thread::setupThisThread();
    guestExec(thread, executableModule, argv, envv);
  });

  thread->handle.join();
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

  bool enableAudioIpmi = false;
  bool asRoot = false;
  bool isSystem = false;
  bool isSafeMode = false;

  int argIndex = 1;
  orbis::initializeAllocator();

  while (argIndex < argc) {
    if (argv[argIndex] == std::string_view("--mount") ||
        argv[argIndex] == std::string_view("-m")) {
      if (argc <= argIndex + 2) {
        usage(argv[0]);
        return 1;
      }

      if (!std::filesystem::is_directory(argv[argIndex + 1])) {
        rx::println(stderr, "Directory '{}' not exists", argv[argIndex + 1]);
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

  setvbuf(stdout, nullptr, _IONBF, 0);
  auto stdinFd = dup(STDIN_FILENO);
  auto stdoutFd = dup(STDOUT_FILENO);
  auto stderrFd = dup(STDERR_FILENO);

  auto logFd = ::open("log-init.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
  dup2(logFd, STDOUT_FILENO);
  dup2(logFd, STDERR_FILENO);
  close(logFd);

  rx::println(stderr, "RPCSX v{}", rx::getVersion().toString());

  // FIXME: determine mode by reading elf file
  orbis::constructAllGlobals();

  setupSigHandlers();
  rx::startWatchdog();

  orbis::allocatePid();
  auto initProcess = orbis::createProcess(nullptr, asRoot ? 1 : 10);
  orbis::vmem::initialize(initProcess);

  auto pmemSize = 9ull * 1024 * 1024 * 1024;
  orbis::g_context->gpuDevice =
      amdgpu::DeviceCtl::createDevice(pmemSize).getOpaque();

  orbis::g_context->deviceEventEmitter = orbis::knew<orbis::EventEmitter>();

  vk::DeviceMemory::NativeHandle handle;
  VK_VERIFY(vk::getDirectMemory().getNativeHandle(handle));
  auto mappable = rx::Mappable::CreateFromNativeHandle(handle);
  rx::AddressRange importedVkMemory;
  if (mappable.map(rx::AddressRange::fromBeginSize(orbis::kMinAddress,
                                                   orbis::vmem::kPageSize),
                   0, rx::mem::Protection::R,
                   orbis::vmem::kPageSize) != std::errc{}) {
    rx::println(stderr, "warning: failed to use Vulkan exported memory, "
                        "switching to imported memory");

    vk::getDirectMemory().free();
    auto [cpuMappable, errc] = rx::Mappable::CreateMemory(pmemSize);

    rx::dieIf(errc != std::errc{},
              "failed to allocate physical memory, errc {}", errc);
    mappable = std::move(cpuMappable);
    auto [addr, mapErrc] = mappable.map(
        pmemSize, 0, rx::mem::Protection::R | rx::mem::Protection::W);
    rx::dieIf(mapErrc != std::errc{}, "failed to map physical memory, errc {}",
              mapErrc);
    vk::getDirectMemory().initFromHost(addr, pmemSize);
    importedVkMemory = rx::AddressRange::fromBeginSize(
        std::bit_cast<std::uintptr_t>(addr), pmemSize);
  } else {
    rx::mem::release(rx::AddressRange::fromBeginSize(orbis::kMinAddress,
                                                     orbis::vmem::kPageSize),
                     orbis::vmem::kPageSize);
  }

  if (auto errc = orbis::pmem::initialize(std::move(mappable), pmemSize);
      errc != orbis::ErrorCode{}) {
    rx::die("pmem initialization failed, {}", errc);
  }
  if (auto errc = orbis::dmem::initialize(); errc != orbis::ErrorCode{}) {
    rx::die("dmem initialization failed, {}", errc);
  }
  if (auto errc = orbis::fmem::initialize(2ull * 1024 * 1024 * 1024);
      errc != orbis::ErrorCode{}) {
    rx::die("fmem initialization failed, {}", errc);
  }

  if (::fork() != 0) {
    rx::attachGpuProcess(::getpid());
    pthread_setname_np(pthread_self(), "rpcsx-gpu");

    int logFd =
        ::open("log-gpu.txt", O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    dup2(logFd, 1);
    dup2(logFd, 2);
    ::close(logFd);

    amdgpu::DeviceCtl{orbis::g_context->gpuDevice}.start();
    return 0;
  }

  rx::attachProcess(::getpid());

  if (importedVkMemory.isValid()) {
    rx::mem::release(importedVkMemory, 0);
  }

  vfs::initialize();

  std::vector<std::string> guestArgv(argv + argIndex, argv + argc);
  if (guestArgv.empty()) {
    guestArgv.emplace_back("/mini-syscore.elf");
    isSystem = true;
    asRoot = true;
  }

  rx::thread::initialize();

  // pthread_setname_np(pthread_self(), "10.MAINTHREAD");

  int status = 0;

  initProcess->onSysEnter = onSysEnter;
  initProcess->onSysExit = onSysExit;
  initProcess->ops = &rx::procOpsTable;
  initProcess->hostPid = ::getpid();
  initProcess->appInfo = {{
      .unk4 = (isSystem ? orbis::slong(0x80000000'00000000) : 0),
  }};

  orbis::BudgetInfo bigAppBudgetInfo[]{
      {
          .resourceId = orbis::BudgetResource::Dmem,
          .flags = 0,
          .item =
              {
                  .total = 0x1'8000'0000,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Vmem,
          .flags = 0,
          .item =
              {
                  .total = 2ul * 1024 * 1024 * 1024,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Fmem,
          .flags = 0,
          .item =
              {
                  .total = 0x1C000000,
              },
      },
      {
          .resourceId = orbis::BudgetResource::CpuSet,
          .flags = 0,
          .item =
              {
                  .total = 7,
              },
      },
      {
          .resourceId = orbis::BudgetResource::File,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Socket,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Equeue,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Pipe,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Device,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
  };

  orbis::BudgetInfo systemBudgetInfo[]{
      {
          .resourceId = orbis::BudgetResource::Dmem,
          .flags = 0,
          .item =
              {
                  .total = 0x1'8000'0000,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Vmem,
          .flags = 0,
          .item =
              {
                  .total = 2ul * 1024 * 1024 * 1024,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Fmem,
          .flags = 0,
          .item =
              {
                  .total = 2ul * 1024 * 1024 * 1024,
              },
      },
      {
          .resourceId = orbis::BudgetResource::CpuSet,
          .flags = 0,
          .item =
              {
                  .total = 8,
              },
      },
      {
          .resourceId = orbis::BudgetResource::File,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Socket,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Equeue,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Pipe,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
      {
          .resourceId = orbis::BudgetResource::Device,
          .flags = 0,
          .item =
              {
                  .total = 4096,
              },
      },
  };

  auto bigAppBudget = orbis::g_context->createProcessTypeBudget(
      orbis::Budget::ProcessType::BigApp, "big app budget", bigAppBudgetInfo);

  // FIXME: define following budgets
  orbis::g_context->createProcessTypeBudget(
      orbis::Budget::ProcessType::MiniApp, "mini-app budget", bigAppBudgetInfo);
  auto systemBudget = orbis::g_context->createProcessTypeBudget(
      orbis::Budget::ProcessType::System, "system budget", systemBudgetInfo);
  orbis::g_context->createProcessTypeBudget(
      orbis::Budget::ProcessType::NonGameMiniApp, "non-game mini-app budget",
      bigAppBudgetInfo);

  if (isSystem) {
    orbis::g_context->safeMode = isSafeMode ? 1 : 0;
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
    initProcess->budgetProcessType = orbis::Budget::ProcessType::System;
    initProcess->budgetId = orbis::g_context->budgets.insert(systemBudget);
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

    initProcess->budgetProcessType = orbis::Budget::ProcessType::BigApp;
    initProcess->budgetId = orbis::g_context->budgets.insert(bigAppBudget);
    initProcess->isInSandbox = true;
  }

  auto mainThread = orbis::createThread(initProcess, "");
  mainThread->hostTid = ::gettid();
  mainThread->nativeHandle = pthread_self();
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

  rx::dieIf(executableModule == nullptr, "Failed to open '{}'", guestArgv[0]);

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

  auto execEnv = guestCreateExecEnv(mainThread, executableModule, isSystem);

  if (isSystem && executableModule->dynType == orbis::DynType::None) {
    orbis::g_context->fwType = orbis::FwType::Ps5;
    executableModule->dynType = orbis::DynType::Ps5;
  }

  guestInitDev(mainThread, stdinFd, stdoutFd, stderrFd);
  guestInitFd(mainThread);

  // data transfer mode
  // 0 - normal
  // 1 - source
  // 2 - ?
  orbis::g_context->regMgrInt[0x2110000] = 0;

  orbis::g_context->regMgrInt[0x20b0000] = 1; // prefer X
  orbis::g_context->regMgrInt[0x2020000] = 1; // region

  // orbis::g_context->regMgrInt[0x2130000] = 0x1601;
  orbis::g_context->regMgrInt[0x2130000] = 0;
  orbis::g_context->regMgrInt[0x73800200] = 1;
  orbis::g_context->regMgrInt[0x73800300] = 0;
  orbis::g_context->regMgrInt[0x73800400] = 0;
  orbis::g_context->regMgrInt[0x73800500] = 0; // enable log

  // user settings
  orbis::g_context->regMgrInt[0x7800100] = 0;
  orbis::g_context->regMgrInt[0x7810100] = 0;
  orbis::g_context->regMgrInt[0x7820100] = 0;
  orbis::g_context->regMgrInt[0x7830100] = 0;
  orbis::g_context->regMgrInt[0x7840100] = 0;
  orbis::g_context->regMgrInt[0x7850100] = 0;
  orbis::g_context->regMgrInt[0x7860100] = 0;
  orbis::g_context->regMgrInt[0x7870100] = 0;
  orbis::g_context->regMgrInt[0x7880100] = 0;
  orbis::g_context->regMgrInt[0x7890100] = 0;
  orbis::g_context->regMgrInt[0x78a0100] = 0;
  orbis::g_context->regMgrInt[0x78b0100] = 0;
  orbis::g_context->regMgrInt[0x78c0100] = 0;
  orbis::g_context->regMgrInt[0x78d0100] = 0;
  orbis::g_context->regMgrInt[0x78e0100] = 0;
  orbis::g_context->regMgrInt[0x78f0100] = 0;

  orbis::g_context->regMgrInt[0x2040000] = 0; // do not require initial setup
  orbis::g_context->regMgrInt[0x2800600] = 0; // IDU version
  orbis::g_context->regMgrInt[0x2860100] = 0; // IDU mode
  orbis::g_context->regMgrInt[0x2860300] = 0; // Arcade mode
  orbis::g_context->regMgrInt[0x7010000] = 0; // auto login
  orbis::g_context->regMgrInt[0x9010000] = 0; // video out color effect

  if (!isSystem) {
    // do IPMI allocations in init process
    ipmi::setWorkerProcess(initProcess);

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
                   {{
                       .titleId = {"NPXS20973"},
                       .unk4 = orbis::slong(0x80000000'00000000),
                   }});
      // confirmed to work and known method of initialization since 5.05
      // version
      if (orbis::g_context->fwType != orbis::FwType::Ps5 &&
          orbis::g_context->fwSdkVersion >= 0x5050000) {
        auto fakeIpmiProcess = orbis::createProcess();
        auto fakeIpmiThread =
            orbis::createThread(fakeIpmiProcess, "SceSysAudioSystemIpc");
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
        data1.pid = fakeIpmiProcess->pid;
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
        std::uint32_t method = orbis::g_context->fwSdkVersion >= 0x8000000
                                   ? 0x1234002c
                                   : 0x1234002b;
        ipmi::audioIpmiClient.sendSyncMessage(method, data1, data2);
      }
    }
  }

  if (orbis::g_context->fwType == orbis::FwType::Ps5 && !isSystem) {
    ipmi::createIpmiServer(initProcess, "SceShareLibIpmiService");
    ipmi::createIpmiServer(initProcess, "SceSysAvControlIpc");
    ipmi::createShm("SceAvControl", 0xa02, 0x1a4, 4096);
    ipmi::createEventFlag("SceAvControlEvf", 0x121, 0);
  }

  mainThread->hostTid = ::gettid();
  mainThread->nativeHandle = pthread_self();
  orbis::g_currentThread = mainThread;
  rx::thread::setupSignalStack();
  rx::thread::setupThisThread();

  status = guestExec(mainThread, execEnv, std::move(executableModule),
                     guestArgv, {});

  rx::thread::deinitialize();

  return status;
}
