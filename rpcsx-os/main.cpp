#include <linux/limits.h>
#include <asm/prctl.h>
#include <fcntl.h>
#include <libunwind.h>
#include <link.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <filesystem>
#include <csignal>
#include <cstddef>
#include <cstdint>

#include <orbis/KernelContext.hpp>
#include <orbis/module.hpp>
#include <orbis/module/Module.hpp>
#include <orbis/sys/sysentry.hpp>
#include <orbis/sys/sysproto.hpp>
#include <orbis/thread/Process.hpp>
#include <orbis/thread/ProcessOps.hpp>
#include <orbis/thread/Thread.hpp>
#include "amdgpu/bridge/bridge.hpp"
#include "rpcsx-os/align.hpp"
#include "rpcsx-os/bridge.hpp"
#include "rpcsx-os/io-device.hpp"
#include "rpcsx-os/io-devices.hpp"
#include "rpcsx-os/linker.hpp"
#include "rpcsx-os/ops.hpp"
#include "rpcsx-os/vfs.hpp"
#include "rpcsx-os/vm.hpp"

static int g_gpuPid;

struct LibcInfo {
  std::uint64_t textBegin = ~static_cast<std::uint64_t>(0);
  std::uint64_t textSize = 0;
};

static LibcInfo libcInfo;

struct ThreadParam {
  void (*startFunc)(void *);
  void *arg;
  orbis::Thread *thread;
};

static thread_local orbis::Thread *g_currentThread = nullptr;

static void printStackTrace(ucontext_t *context, int fileno) {
  unw_cursor_t cursor;

  char buffer[1024];

  if (int r = unw_init_local2(&cursor, context, UNW_INIT_SIGNAL_FRAME)) {
    int len = snprintf(buffer, sizeof(buffer), "unw_init_local: %s\n",
                       unw_strerror(r));
    write(fileno, buffer, len);
    return;
  }

  char functionName[256];

  int count = 0;
  do {
    unw_word_t ip;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);

    unw_word_t off;
    int proc_res =
      unw_get_proc_name(&cursor, functionName, sizeof(functionName), &off);

    Dl_info dlinfo;
    int dladdr_res = ::dladdr((void *)ip, &dlinfo);

    unsigned long baseAddress =
      dladdr_res != 0 ? reinterpret_cast<std::uint64_t>(dlinfo.dli_fbase) : 0;

    int len = snprintf(buffer, sizeof(buffer), "%3d: %s+%p: %s(%lx)+%#lx\n",
                       count, (dladdr_res != 0 ? dlinfo.dli_fname : "??"),
                       reinterpret_cast<void *>(ip - baseAddress),
                       (proc_res == 0 ? functionName : "??"),
                       reinterpret_cast<unsigned long>(
                         proc_res == 0 ? ip - baseAddress - off : 0),
                       static_cast<unsigned long>(proc_res == 0 ? off : 0));
    write(fileno, buffer, len);
    count++;
  } while (unw_step(&cursor) > 0 && count < 32);
}

static std::size_t printAddressLocation(char *dest, std::size_t destLen,
                                        orbis::Thread *thread,
                                        std::uint64_t address) {
  if (thread == nullptr || address == 0) {
    return 0;
  }

  for (auto [id, module] : thread->tproc->modulesMap) {
    auto moduleBase = reinterpret_cast<std::uint64_t>(module->base);
    if (moduleBase > address || moduleBase + module->size <= address) {
      continue;
    }

    return std::snprintf(dest, destLen, "%s+%#" PRIx64, module->soName,
                         address - moduleBase);
  }

  return 0;
}

static void printStackTrace(ucontext_t *context, orbis::Thread *thread,
                            int fileno) {
  unw_cursor_t cursor;

  char buffer[1024];

  if (int r = unw_init_local2(&cursor, context, UNW_INIT_SIGNAL_FRAME)) {
    int len = snprintf(buffer, sizeof(buffer), "unw_init_local: %s\n",
                       unw_strerror(r));
    write(fileno, buffer, len);
    return;
  }

  int count = 0;
  char functionName[256];
  do {
    unw_word_t ip;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    std::size_t offset = 0;

    offset +=
      std::snprintf(buffer + offset, sizeof(buffer) - offset, "%3d: ", count);

    if (auto loc = printAddressLocation(buffer + offset,
                                        sizeof(buffer) - offset, thread, ip)) {
      offset += loc;
      offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "\n");
    } else {
      unw_word_t off;
      int proc_res =
        unw_get_proc_name(&cursor, functionName, sizeof(functionName), &off);

      Dl_info dlinfo;
      int dladdr_res = ::dladdr((void *)ip, &dlinfo);

      unsigned long baseAddress =
        dladdr_res != 0 ? reinterpret_cast<std::uint64_t>(dlinfo.dli_fbase)
        : 0;

      offset = snprintf(buffer, sizeof(buffer), "%3d: %s+%p: %s(%lx)+%#lx\n",
                        count, (dladdr_res != 0 ? dlinfo.dli_fname : "??"),
                        reinterpret_cast<void *>(ip - baseAddress),
                        (proc_res == 0 ? functionName : "??"),
                        reinterpret_cast<unsigned long>(
                          proc_res == 0 ? ip - baseAddress - off : 0),
                        static_cast<unsigned long>(proc_res == 0 ? off : 0));
    }

    write(fileno, buffer, offset);
    count++;
  } while (unw_step(&cursor) > 0 && count < 32);
}

__attribute__((no_stack_protector)) static void
handle_signal(int sig, siginfo_t *info, void *ucontext) {
  std::uint64_t hostFs = _readgsbase_u64();
  if (hostFs != 0) {
    _writefsbase_u64(hostFs);
  }

  // syscall(SYS_arch_prctl, ARCH_GET_GS, &hostFs);
  // syscall(SYS_arch_prctl, ARCH_SET_FS, hostFs);

  if (sig == SIGSYS) {
    // printf("%x: %x\n", tid, thread->tid);
    g_currentThread->context = reinterpret_cast<ucontext_t *>(ucontext);
    orbis::syscall_entry(g_currentThread);
    _writefsbase_u64(g_currentThread->fsBase);
    // syscall(SYS_arch_prctl, ARCH_SET_FS, g_currentThread->regs.fs);
    return;
  }

  if (g_gpuPid > 0) {
    // stop gpu thread
    ::kill(g_gpuPid, SIGINT);
  }

  if (sig != SIGINT) {
    char buf[128] = "";
    int len = snprintf(buf, sizeof(buf), " [%s] %u: Signal address=%p\n",
                       g_currentThread ? "guest" : "host",
                       g_currentThread ? g_currentThread->tid : ::gettid(),
                       info->si_addr);
    write(2, buf, len);

    if (std::size_t printed = printAddressLocation(
      buf, sizeof(buf), g_currentThread, (std::uint64_t)info->si_addr)) {
      printed += std::snprintf(buf + printed, sizeof(buf) - printed, "\n");
      write(2, buf, printed);
    }


    if (g_currentThread) {
      printStackTrace(reinterpret_cast<ucontext_t *>(ucontext), g_currentThread,
                      2);
    } else {
      printStackTrace(reinterpret_cast<ucontext_t *>(ucontext), 2);
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

static void setupSigHandlers() {
  stack_t ss;

  ss.ss_sp = malloc(SIGSTKSZ);
  if (ss.ss_sp == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  ss.ss_size = SIGSTKSZ;
  ss.ss_flags = 0;

  if (sigaltstack(&ss, NULL) == -1) {
    perror("sigaltstack");
    exit(EXIT_FAILURE);
  }

  struct sigaction act;
  sigset_t mask;
  memset(&act, 0, sizeof(act));
  sigemptyset(&mask);

  act.sa_sigaction = handle_signal;
  act.sa_flags = SA_SIGINFO | SA_ONSTACK;
  act.sa_mask = mask;

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

__attribute__((no_stack_protector)) static void *
emuThreadEntryPoint(void *paramsVoid) {
  auto params = *reinterpret_cast<ThreadParam *>(paramsVoid);
  delete reinterpret_cast<ThreadParam *>(paramsVoid);

  g_currentThread = params.thread;

  std::uint64_t hostFs;
  syscall(SYS_arch_prctl, ARCH_GET_FS, &hostFs);
  syscall(SYS_arch_prctl, ARCH_SET_GS, hostFs);

  if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
            libcInfo.textBegin, libcInfo.textSize, nullptr)) {
    perror("prctl failed\n");
    exit(-1);
  }

  syscall(SYS_arch_prctl, ARCH_SET_FS, params.thread->fsBase);
  params.startFunc(params.arg);
  syscall(SYS_arch_prctl, ARCH_SET_FS, hostFs);

  return nullptr;
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

static void createEmuThread(orbis::Thread &thread, uint64_t entryPoint,
                            uint64_t hostStackSize, uint64_t arg) {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstack(&attr, thread.stackStart, hostStackSize);

  pthread_t pthread;
  auto params = new ThreadParam;

  params->startFunc = (void (*)(void *))entryPoint;
  params->arg = (void *)arg;
  params->thread = &thread;

  pthread_create(&pthread, &attr, emuThreadEntryPoint, params);
  pthread_join(pthread, nullptr);
}

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
  if (true || !g_traceSyscalls) {
    return;
  }
  std::printf("   [%u] ", thread->tid);

  if (auto name = getSyscallName(thread, id)) {
    std::printf("%s(", name);
  } else {
    std::printf("sys_%u(", id);
  }

  for (int i = 0; i < argsCount; ++i) {
    if (i != 0) {
      std::printf(", ");
    }

    std::printf("%#lx", args[i]);
  }

  std::printf(")\n");
}

static void onSysExit(orbis::Thread *thread, int id, uint64_t *args,
                      int argsCount, orbis::SysResult result) {
  if (!result.isError() && !g_traceSyscalls) {
    return;
  }

  std::printf("%c: [%u] ", result.isError() ? 'E' : 'S', thread->tid);

  if (auto name = getSyscallName(thread, id)) {
    std::printf("%s(", name);
  } else {
    std::printf("sys_%u(", id);
  }

  for (int i = 0; i < argsCount; ++i) {
    if (i != 0) {
      std::printf(", ");
    }

    std::printf("%#lx", args[i]);
  }

  std::printf(") -> Status %d, Value %lx:%lx\n", result.value(),
              thread->retval[0], thread->retval[1]);
}

static int ps4Exec(orbis::Process *mainProcess,
                   orbis::utils::Ref<orbis::Module> executableModule,
                   std::span<const char *> argv, std::span<const char *> envp) {
  mainProcess->sysent = &orbis::ps4_sysvec;
  mainProcess->ops = &rx::procOpsTable;

  orbis::Thread mainThread;
  mainThread.tproc = mainProcess;
  mainThread.tid = mainProcess->pid;
  mainThread.state = orbis::ThreadState::RUNNING;

  const auto stackEndAddress = 0x7'ffff'c000ull;
  const auto stackSize = 0x40000 * 16;
  auto stackStartAddress = stackEndAddress - stackSize;
  mainThread.stackStart =
    rx::vm::map(reinterpret_cast<void *>(stackStartAddress), stackSize,
                rx::vm::kMapProtCpuWrite | rx::vm::kMapProtCpuRead,
                rx::vm::kMapFlagAnonymous | rx::vm::kMapFlagFixed |
                rx::vm::kMapFlagPrivate | rx::vm::kMapFlagStack);

  mainThread.stackEnd =
    reinterpret_cast<std::byte *>(mainThread.stackStart) + stackSize;

  rx::vfs::mount("/dev/dmem0", createDmemCharacterDevice(0));
  rx::vfs::mount("/dev/dmem1", createDmemCharacterDevice(1));
  rx::vfs::mount("/dev/dmem2", createDmemCharacterDevice(2));
  rx::vfs::mount("/dev/stdout", createStdoutCharacterDevice());
  rx::vfs::mount("/dev/stderr", createStderrCharacterDevice());
  rx::vfs::mount("/dev/stdin", createStdinCharacterDevice());
  rx::vfs::mount("/dev/zero", createZeroCharacterDevice());
  rx::vfs::mount("/dev/null", createNullCharacterDevice());
  rx::vfs::mount("/dev/dipsw", createDipswCharacterDevice());
  rx::vfs::mount("/dev/dce", createDceCharacterDevice());
  rx::vfs::mount("/dev/hmd_cmd", createHmdCmdCharacterDevice());
  rx::vfs::mount("/dev/hmd_snsr", createHmdSnsrCharacterDevice());
  rx::vfs::mount("/dev/hmd_3da", createHmd3daCharacterDevice());
  rx::vfs::mount("/dev/hmd_dist", createHmdMmapCharacterDevice());
  rx::vfs::mount("/dev/hid", createHidCharacterDevice());
  rx::vfs::mount("/dev/gc", createGcCharacterDevice());
  rx::vfs::mount("/dev/rng", createRngCharacterDevice());

  rx::procOpsTable.open(&mainThread, "/dev/stdin", 0, 0);
  rx::procOpsTable.open(&mainThread, "/dev/stdout", 0, 0);
  rx::procOpsTable.open(&mainThread, "/dev/stderr", 0, 0);

  std::vector<std::uint64_t> argvOffsets;
  std::vector<std::uint64_t> envpOffsets;

  auto libkernel = rx::linker::loadModuleFile(
    "/system/common/lib/libkernel_sys.sprx", mainProcess);

  // *reinterpret_cast<std::uint32_t *>(
  //     reinterpret_cast<std::byte *>(libkernel->base) + 0x6c2e4) = ~0;

  StackWriter stack{ reinterpret_cast<std::uint64_t>(mainThread.stackEnd) };

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

  ucontext_t currentContext;
  getcontext(&currentContext);

  createEmuThread(
    mainThread, libkernel->entryPoint,
    utils::alignDown(
      stack.address -
      reinterpret_cast<std::uint64_t>(mainThread.stackStart) - 0x1000,
      rx::vm::kPageSize),
    sp);
  return 0;
}

static void usage(const char *argv0) {
  std::printf("%s [<options>...] <virtual path to elf> [args...]\n", argv0);
  std::printf("  options:\n");
  std::printf("    -m, --mount <host path> <virtual path>\n");
  std::printf("    -o, --override <original module name> <virtual path to overriden module>\n");
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

  std::printf("filename is '%s'\n", std::filesystem::path(path).filename().c_str());

  return std::filesystem::path(path).filename() == "rpcsx-gpu";
}
static void runRpsxGpu() {
  const char *cmdBufferName = "/rpcsx-gpu-cmds";
  amdgpu::bridge::BridgeHeader *bridgeHeader = amdgpu::bridge::openShmCommandBuffer(cmdBufferName);

  if (bridgeHeader != nullptr && bridgeHeader->pullerPid > 0 && isRpsxGpuPid(bridgeHeader->pullerPid)) {
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
    const char *argv[] = { rpcsxGpuPath.c_str(), nullptr };

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

  auto processPhdr = [](struct dl_phdr_info *info, size_t, void *data) {
    auto path = std::string_view(info->dlpi_name);
    auto slashPos = path.rfind('/');
    if (slashPos == std::string_view::npos) {
      return 0;
    }

    auto name = path.substr(slashPos + 1);
    if (name.starts_with("libc.so")) {
      std::printf("%s\n", std::string(name).c_str());
      auto libcInfo = reinterpret_cast<LibcInfo *>(data);

      for (std::size_t i = 0; i < info->dlpi_phnum; ++i) {
        auto &phdr = info->dlpi_phdr[i];

        if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X) == PF_X) {
          libcInfo->textBegin =
            std::min(libcInfo->textBegin, phdr.p_vaddr + info->dlpi_addr);
          libcInfo->textSize = std::max(libcInfo->textSize, phdr.p_memsz);
        }
      }

      return 1;
    }

    return 0;
    };

  dl_iterate_phdr(processPhdr, &libcInfo);

  std::printf("libc text %zx-%zx\n", libcInfo.textBegin,
              libcInfo.textBegin + libcInfo.textSize);

  setupSigHandlers();
  // rx::vm::printHostStats();

  rx::vfs::initialize();

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

      rx::vfs::mount(argv[argIndex + 2], createHostIoDevice(argv[argIndex + 1]));
      argIndex += 3;
      continue;
    }

    if (argv[argIndex] == std::string_view("--trace")) {
      argIndex++;
      g_traceSyscalls = true;
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


    break;
  }

  if (argIndex >= argc) {
    usage(argv[0]);
    return 1;
  }

  rx::vm::initialize();
  runRpsxGpu();

  // rx::vm::printHostStats();
  auto initProcess = orbis::g_context.createProcess(10);
  initProcess->sysent = &orbis::ps4_sysvec;
  initProcess->onSysEnter = onSysEnter;
  initProcess->onSysExit = onSysExit;
  auto executableModule =
    rx::linker::loadModuleFile(argv[argIndex], initProcess);

  if (executableModule == nullptr) {
    std::fprintf(stderr, "Failed to open '%s'\n", argv[argIndex]);
    std::abort();
  }

  initProcess->processParam = executableModule->processParam;
  initProcess->processParamSize = executableModule->processParamSize;

  int status = 0;

  if (executableModule->type == rx::linker::kElfTypeSceDynExec ||
      executableModule->type == rx::linker::kElfTypeSceExec) {
    status = ps4Exec(initProcess, std::move(executableModule),
                     std::span(argv + argIndex, argc - argIndex),
                     std::span<const char *>());
  } else {
    std::fprintf(stderr, "Unexpected executable type\n");
    status = 1;
  }

  // entryPoint();

  // rx::vm::printHostStats();
  rx::vm::deinitialize();

  return status;
}
