#include "thread.hpp"
#include "align.hpp"
#include "orbis/sys/sysentry.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include <asm/prctl.h>
#include <csignal>
#include <immintrin.h>
#include <link.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <ucontext.h>
#include <unistd.h>
#include <xbyak/xbyak.h>

static auto setContext = [] {
  struct SetContext : Xbyak::CodeGenerator {
    SetContext() {
      mov(rbp, rdi);
      mov(rax, qword[rbp + REG_RAX * sizeof(unsigned long long)]);
      mov(rdi, qword[rbp + REG_RDI * sizeof(unsigned long long)]);
      mov(rdx, qword[rbp + REG_RDX * sizeof(unsigned long long)]);
      mov(rcx, qword[rbp + REG_RCX * sizeof(unsigned long long)]);
      mov(rbx, qword[rbp + REG_RBX * sizeof(unsigned long long)]);
      mov(rsi, qword[rbp + REG_RSI * sizeof(unsigned long long)]);
      mov(rsp, qword[rbp + REG_RSP * sizeof(unsigned long long)]);

      mov(rbp, qword[rbp + REG_RIP * sizeof(unsigned long long)]);
      call(rbp);
    }
  } static setContextStorage;

  return setContextStorage.getCode<void (*)(const mcontext_t &)>();
}();

static __attribute__((no_stack_protector)) void
handleSigSys(int sig, siginfo_t *info, void *ucontext) {
  if (auto hostFs = _readgsbase_u64()) {
    _writefsbase_u64(hostFs);
  }

  // rx::printStackTrace(reinterpret_cast<ucontext_t *>(ucontext),
  // rx::thread::g_current, 1);
  auto thread = orbis::g_currentThread;
  auto prevContext = std::exchange(thread->context, ucontext);
  orbis::syscall_entry(thread);
  thread = orbis::g_currentThread;
  thread->context = prevContext;
  _writefsbase_u64(thread->fsBase);
}

void rx::thread::initialize() {
  struct sigaction act {};
  act.sa_sigaction = handleSigSys;
  act.sa_flags = SA_SIGINFO | SA_ONSTACK;

  if (sigaction(SIGSYS, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }
}

void rx::thread::deinitialize() {}

void rx::thread::setupSignalStack() {
  stack_t ss{};

  auto sigStackSize = std::max<std::size_t>(
      SIGSTKSZ, ::utils::alignUp(64 * 1024 * 1024, sysconf(_SC_PAGE_SIZE)));

  ss.ss_sp = malloc(sigStackSize);
  if (ss.ss_sp == NULL) {
    perror("malloc");
    std::exit(EXIT_FAILURE);
  }

  ss.ss_size = sigStackSize;
  ss.ss_flags = 1 << 31;

  if (sigaltstack(&ss, NULL) == -1) {
    perror("sigaltstack");
    std::exit(EXIT_FAILURE);
  }
}

void rx::thread::setupThisThread() {
  sigset_t unblockSigs{};
  sigset_t oldSigmask{};
  sigaddset(&unblockSigs, SIGSYS);
  if (pthread_sigmask(SIG_UNBLOCK, &unblockSigs, &oldSigmask)) {
    perror("pthread_sigmask failed\n");
    std::exit(-1);
  }

  if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
            (void *)0x100'0000'0000, ~0ull - 0x100'0000'0000, nullptr)) {
    perror("prctl failed\n");
    std::exit(-1);
  }
}

void rx::thread::invoke(orbis::Thread *thread) {
  orbis::g_currentThread = thread;

  std::uint64_t hostFs = _readfsbase_u64();
  _writegsbase_u64(hostFs);

  _writefsbase_u64(thread->fsBase);
  auto context = reinterpret_cast<ucontext_t *>(thread->context);

  setContext(context->uc_mcontext);
  _writefsbase_u64(hostFs);
}
