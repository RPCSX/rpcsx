#include "align.hpp"
#include "amdgpu/bridge/bridge.hpp"
#include "backtrace.hpp"
#include "bridge.hpp"
#include "io-device.hpp"
#include "io-devices.hpp"
#include "linker.hpp"
#include "ops.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vm.hpp"

#include <atomic>
#include <elf.h>
#include <filesystem>
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
#include <ucontext.h>

#include <csignal>
#include <cstddef>
#include <cstdint>

static int g_gpuPid;

__attribute__((no_stack_protector)) static void
handle_signal(int sig, siginfo_t *info, void *ucontext) {
  if (auto hostFs = _readgsbase_u64()) {
    _writefsbase_u64(hostFs);
  }

  auto signalAddress = reinterpret_cast<std::uintptr_t>(info->si_addr);

  if (orbis::g_currentThread != nullptr && sig == SIGSEGV &&
      signalAddress >= 0x40000 && signalAddress < 0x100'0000'0000) {
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
        auto flags = bridge->cachePages[page].load(std::memory_order::relaxed);

        if ((flags & amdgpu::bridge::kPageReadWriteLock) != 0) {
          if ((flags & amdgpu::bridge::kPageLazyLock) != 0) {
            if (std::uint32_t gpuCommand = 0;
                !bridge->gpuCacheCommand.compare_exchange_weak(gpuCommand,
                                                               page)) {
              continue;
            }

            while (!bridge->cachePages[page].compare_exchange_weak(
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

        if (bridge->cachePages[page].compare_exchange_weak(
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

  if (g_gpuPid > 0) {
    // stop gpu thread
    ::kill(g_gpuPid, SIGINT);
  }

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

  struct sigaction act {};
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
  auto sigStackSize = std::max<std::size_t>(
      SIGSTKSZ, utils::alignUp(8 * 1024 * 1024, sysconf(_SC_PAGE_SIZE)));

  stack_t ss{};
  ss.ss_sp = malloc(sigStackSize);
  if (ss.ss_sp == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  ss.ss_size = sigStackSize;
  ss.ss_flags = 1 << 31;

  if (sigaltstack(&ss, NULL) == -1) {
    perror("sigaltstack");
    exit(EXIT_FAILURE);
  }

  struct sigaction act {};
  act.sa_sigaction = handle_signal;
  act.sa_flags = SA_SIGINFO | SA_ONSTACK;

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
  if (!g_traceSyscalls && thread->tid < 10000) {
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
  if (!result.isError() && !g_traceSyscalls && thread->tid < 10000) {
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

  thread->where();
  funlockfile(stderr);
}

static int ps4Exec(orbis::Thread *mainThread,
                   orbis::utils::Ref<orbis::Module> executableModule,
                   std::span<const char *> argv, std::span<const char *> envp) {
  const auto stackEndAddress = 0x7'ffff'c000ull;
  const auto stackSize = 0x40000 * 16;
  auto stackStartAddress = stackEndAddress - stackSize;
  mainThread->stackStart =
      rx::vm::map(reinterpret_cast<void *>(stackStartAddress), stackSize,
                  rx::vm::kMapProtCpuWrite | rx::vm::kMapProtCpuRead,
                  rx::vm::kMapFlagAnonymous | rx::vm::kMapFlagFixed |
                      rx::vm::kMapFlagPrivate | rx::vm::kMapFlagStack);

  mainThread->stackEnd =
      reinterpret_cast<std::byte *>(mainThread->stackStart) + stackSize;

  auto dmem1 = createDmemCharacterDevice(1);
  orbis::g_context.dmemDevice = dmem1;

  auto stdoutFd = ::open("stdout.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
  auto stderrFd = ::open("stderr.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);

  rx::vfs::addDevice("dmem0", createDmemCharacterDevice(0));
  rx::vfs::addDevice("npdrm", createNpdrmCharacterDevice());
  rx::vfs::addDevice("icc_configuration",
                     createIccConfigurationCharacterDevice());
  rx::vfs::addDevice("console", createConsoleCharacterDevice());
  rx::vfs::addDevice("camera", createCameraCharacterDevice());
  rx::vfs::addDevice("dmem1", dmem1);
  rx::vfs::addDevice("dmem2", createDmemCharacterDevice(2));
  rx::vfs::addDevice("stdout", createFdWrapDevice(stdoutFd));
  rx::vfs::addDevice("stderr", createFdWrapDevice(stderrFd));
  rx::vfs::addDevice("deci_stdin", createFdWrapDevice(STDIN_FILENO));
  rx::vfs::addDevice("deci_stdout", createFdWrapDevice(stdoutFd));
  rx::vfs::addDevice("deci_stderr", createFdWrapDevice(stderrFd));
  rx::vfs::addDevice("stdin", createFdWrapDevice(STDIN_FILENO));
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
  rx::vfs::addDevice("mbus", createMBusCharacterDevice());
  rx::vfs::addDevice("metadbg", createMetaDbgCharacterDevice());
  rx::vfs::addDevice("bt", createBtCharacterDevice());
  rx::vfs::addDevice("xpt0", createXptCharacterDevice());
  rx::vfs::addDevice("cd0", createXptCharacterDevice());
  rx::vfs::addDevice("notification0", createNotificationCharacterDevice(0));
  rx::vfs::addDevice("notification1", createNotificationCharacterDevice(1));
  rx::vfs::addDevice("notification2", createNotificationCharacterDevice(2));
  rx::vfs::addDevice("notification3", createNotificationCharacterDevice(3));
  rx::vfs::addDevice("notification4", createNotificationCharacterDevice(4));
  rx::vfs::addDevice("notification5", createNotificationCharacterDevice(5));

  orbis::Ref<orbis::File> stdinFile;
  orbis::Ref<orbis::File> stdoutFile;
  orbis::Ref<orbis::File> stderrFile;
  rx::procOpsTable.open(mainThread, "/dev/stdin", 0, 0, &stdinFile);
  rx::procOpsTable.open(mainThread, "/dev/stdout", 0, 0, &stdoutFile);
  rx::procOpsTable.open(mainThread, "/dev/stderr", 0, 0, &stderrFile);

  mainThread->tproc->fileDescriptors.insert(stdinFile);
  mainThread->tproc->fileDescriptors.insert(stdoutFile);
  mainThread->tproc->fileDescriptors.insert(stderrFile);

  orbis::g_context.shmDevice = createShmDevice();
  orbis::g_context.blockpoolDevice = createBlockPoolDevice();

  std::vector<std::uint64_t> argvOffsets;
  std::vector<std::uint64_t> envpOffsets;

  auto libSceLibcInternal = rx::linker::loadModuleFile(
      "/system/common/lib/libSceLibcInternal.sprx", mainThread);

  if (libSceLibcInternal == nullptr) {
    std::fprintf(stderr, "libSceLibcInternal not found\n");
    return 1;
  }

  libSceLibcInternal->id =
      mainThread->tproc->modulesMap.insert(libSceLibcInternal);

  auto libkernel = rx::linker::loadModuleFile(
      "/system/common/lib/libkernel_sys.sprx", mainThread);

  if (libkernel == nullptr) {
    std::fprintf(stderr, "libkernel not found\n");
    return 1;
  }

  libkernel->id = mainThread->tproc->modulesMap.insert(libkernel);

  // *reinterpret_cast<std::uint32_t *>(
  //     reinterpret_cast<std::byte *>(libkernel->base) + 0x6c2e4) = ~0;

  // *reinterpret_cast<std::uint32_t *>(
  //     reinterpret_cast<std::byte *>(libkernel->base) + 0x71300) = ~0;

  StackWriter stack{reinterpret_cast<std::uint64_t>(mainThread->stackEnd)};

  for (auto elem : argv) {
    argvOffsets.push_back(stack.pushString(elem));
  }

  argvOffsets.push_back(0);

  for (auto elem : envp) {
    envpOffsets.push_back(stack.pushString(elem));
  }

  envpOffsets.push_back(0);

  // clang-format off
  std::uint64_t auxv[] = {
      AT_ENTRY, executableModule->entryPoint,
      AT_BASE, reinterpret_cast<std::uint64_t>(libkernel->base),
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
  ;
  context->uc_mcontext.gregs[REG_RIP] = libkernel->entryPoint;

  mainThread->context = context;
  rx::thread::invoke(mainThread);
  std::abort();
}

static void usage(const char *argv0) {
  std::printf("%s [<options>...] <virtual path to elf> [args...]\n", argv0);
  std::printf("  options:\n");
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

static bool isRpsxGpuPid(int pid) {
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

  return std::filesystem::path(path).filename() == "rpcsx-gpu";
}
static void runRpsxGpu() {
  const char *cmdBufferName = "/rpcsx-gpu-cmds";
  amdgpu::bridge::BridgeHeader *bridgeHeader =
      amdgpu::bridge::openShmCommandBuffer(cmdBufferName);

  if (bridgeHeader != nullptr && bridgeHeader->pullerPid > 0 &&
      isRpsxGpuPid(bridgeHeader->pullerPid)) {
    bridgeHeader->pusherPid = ::getpid();
    g_gpuPid = bridgeHeader->pullerPid;
    rx::bridge = bridgeHeader;
    return;
  }

  std::printf("Starting rpcsx-gpu\n");

  if (bridgeHeader == nullptr) {
    bridgeHeader = amdgpu::bridge::createShmCommandBuffer(cmdBufferName);
  }
  bridgeHeader->pusherPid = ::getpid();
  rx::bridge = bridgeHeader;

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

int main(int argc, const char *argv[]) {
  if (argc == 2) {
    if (std::strcmp(argv[1], "-h") == 0 ||
        std::strcmp(argv[1], "--help") == 0) {
      usage(argv[0]);
      return 1;
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

      rx::vfs::mount(argv[argIndex + 2],
                     createHostIoDevice(argv[argIndex + 1]));
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
  rx::vm::initialize();
  runRpsxGpu();

  if (enableAudio) {
    orbis::g_context.audioOut = orbis::knew<orbis::AudioOut>();
  }

  // rx::vm::printHostStats();
  orbis::g_context.allocatePid();
  auto initProcess = orbis::g_context.createProcess(asRoot ? 1 : 11);
  pthread_setname_np(pthread_self(), "11.MAINTHREAD");

  std::thread{[] {
    pthread_setname_np(pthread_self(), "Bridge");
    auto bridge = rx::bridge.header;

    std::vector<std::uint64_t> fetchedCommands;
    fetchedCommands.reserve(std::size(bridge->cacheCommands));

    while (true) {
      for (auto &command : bridge->cacheCommands) {
        std::uint64_t value = command.load(std::memory_order::relaxed);

        if (value != 0) {
          fetchedCommands.push_back(value);
          command.store(0, std::memory_order::relaxed);
        }
      }

      if (fetchedCommands.empty()) {
        continue;
      }

      for (auto command : fetchedCommands) {
        auto page = static_cast<std::uint32_t>(command);
        auto count = static_cast<std::uint32_t>(command >> 32) + 1;

        auto pageFlags =
            bridge->cachePages[page].load(std::memory_order::relaxed);

        auto address =
            static_cast<std::uint64_t>(page) * amdgpu::bridge::kHostPageSize;
        auto origVmProt = rx::vm::getPageProtection(address);
        int prot = 0;

        if (origVmProt & rx::vm::kMapProtCpuRead) {
          prot |= PROT_READ;
        }
        if (origVmProt & rx::vm::kMapProtCpuWrite) {
          prot |= PROT_WRITE;
        }
        if (origVmProt & rx::vm::kMapProtCpuExec) {
          prot |= PROT_EXEC;
        }

        if (pageFlags & amdgpu::bridge::kPageReadWriteLock) {
          prot &= ~(PROT_READ | PROT_WRITE);
        } else if (pageFlags & amdgpu::bridge::kPageWriteWatch) {
          prot &= ~PROT_WRITE;
        }

        // std::fprintf(stderr, "protection %lx-%lx\n", address,
        //              address + amdgpu::bridge::kHostPageSize * count);
        if (::mprotect(reinterpret_cast<void *>(address),
                       amdgpu::bridge::kHostPageSize * count, prot)) {
          perror("protection failed");
          std::abort();
        }
      }

      fetchedCommands.clear();
    }
  }}.detach();

  int status = 0;

  initProcess->sysent = &orbis::ps4_sysvec;
  initProcess->onSysEnter = onSysEnter;
  initProcess->onSysExit = onSysExit;
  initProcess->ops = &rx::procOpsTable;
  initProcess->isSystem = isSystem;

  auto [baseId, mainThread] = initProcess->threadsMap.emplace();
  mainThread->tproc = initProcess;
  mainThread->tid = initProcess->pid + baseId;
  mainThread->state = orbis::ThreadState::RUNNING;

  auto executableModule =
      rx::linker::loadModuleFile(argv[argIndex], mainThread);

  if (executableModule == nullptr) {
    std::fprintf(stderr, "Failed to open '%s'\n", argv[argIndex]);
    std::abort();
  }

  executableModule->id = initProcess->modulesMap.insert(executableModule);
  initProcess->processParam = executableModule->processParam;
  initProcess->processParamSize = executableModule->processParamSize;

  if (executableModule->type == rx::linker::kElfTypeSceDynExec ||
      executableModule->type == rx::linker::kElfTypeSceExec ||
      executableModule->type == rx::linker::kElfTypeExec) {
    status = ps4Exec(mainThread, std::move(executableModule),
                     std::span(argv + argIndex, argc - argIndex),
                     std::span<const char *>());
  } else {
    std::fprintf(stderr, "Unexpected executable type\n");
    status = 1;
  }

  // rx::vm::printHostStats();
  rx::vm::deinitialize();
  rx::thread::deinitialize();

  return status;
}
