#include "thread.hpp"
#include "backtrace.hpp"
#include "orbis/sys/sysentry.hpp"
#include <asm/prctl.h>
#include <csignal>
#include <immintrin.h>
#include <link.h>
#include <linux/prctl.h>
#include <string>
#include <sys/prctl.h>
#include <ucontext.h>
#include <unistd.h>
#include <xbyak/xbyak.h>

thread_local orbis::Thread *rx::thread::g_current = nullptr;

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
  auto prevContext = std::exchange(rx::thread::g_current->context, ucontext);
  orbis::syscall_entry(rx::thread::g_current);
  rx::thread::g_current->context = prevContext;
  _writefsbase_u64(rx::thread::g_current->fsBase);
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

void rx::thread::invoke(orbis::Thread *thread) {
  g_current = thread;

  sigset_t unblockSigs{};
  sigset_t oldSigmask{};
  sigaddset(&unblockSigs, SIGSYS);
  if (pthread_sigmask(SIG_UNBLOCK, &unblockSigs, &oldSigmask)) {
    perror("pthread_sigmask failed\n");
    exit(-1);
  }

  std::uint64_t hostFs = _readfsbase_u64();
  _writegsbase_u64(hostFs);

  if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
            (void *)0x100'0000'0000, ~0ull - 0x100'0000'0000, nullptr)) {
    perror("prctl failed\n");
    exit(-1);
  }

  _writefsbase_u64(thread->fsBase);
  auto context = reinterpret_cast<ucontext_t *>(thread->context);

  setContext(context->uc_mcontext);
  _writefsbase_u64(hostFs);
}
