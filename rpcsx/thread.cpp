#include "thread.hpp"
#include "orbis-config.hpp"
#include "orbis/sys/sysentry.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/mem.hpp"
#include "rx/print.hpp"
#include <asm/prctl.h>
#include <csignal>
#include <immintrin.h>
#include <link.h>
#include <linux/prctl.h>
#include <rx/align.hpp>
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

  if ((std::uint64_t)&thread < orbis::kMaxAddress) {
    ORBIS_LOG_ERROR("unexpected sigsys signal stack", thread->tid, sig,
                    (std::uint64_t)&thread);
    std::abort();
  }

  orbis::syscall_entry(thread);

  thread = orbis::g_currentThread;
  thread->context = prevContext;
  _writefsbase_u64(thread->fsBase);
}

__attribute__((no_stack_protector)) static void
handleSigUser(int sig, siginfo_t *info, void *ucontext) {
  if (auto hostFs = _readgsbase_u64()) {
    _writefsbase_u64(hostFs);
  }

  auto context = reinterpret_cast<ucontext_t *>(ucontext);
  bool inGuestCode = context->uc_mcontext.gregs[REG_RIP] < orbis::kMaxAddress;
  auto thread = orbis::g_currentThread;

  if ((std::uint64_t)&context < orbis::kMaxAddress) {
    ORBIS_LOG_ERROR("unexpected sigusr signal stack", thread->tid, sig,
                    inGuestCode, (std::uint64_t)&context);
    std::abort();
  }

  int guestSignal = info->si_value.sival_int;

  if (guestSignal == -1) {
    // ORBIS_LOG_ERROR("suspending thread", thread->tid, inGuestCode);

    if (inGuestCode) {
      thread->unblock();
    }

    auto [value, locked] = thread->suspendFlags.fetch_op([](unsigned &value) {
      if ((value & ~orbis::kThreadSuspendFlag) != 0) {
        value |= orbis::kThreadSuspendFlag;
        return true;
      }
      return false;
    });

    if (locked) {
      thread->suspendFlags.notify_all();
      value |= orbis::kThreadSuspendFlag;

      while (true) {
        while ((value & ~orbis::kThreadSuspendFlag) != 0) {
          thread->suspendFlags.wait(value);
          value = thread->suspendFlags.load(std::memory_order::relaxed);
        }

        bool unlocked = thread->suspendFlags.op([](unsigned &value) {
          if ((value & ~orbis::kThreadSuspendFlag) == 0) {
            value &= ~orbis::kThreadSuspendFlag;
            return true;
          }

          return false;
        });

        if (unlocked) {
          thread->suspendFlags.notify_all();
          break;
        }
      }
    }

    if (inGuestCode) {
      thread->block();
    }

    // ORBIS_LOG_ERROR("thread wake", thread->tid);
  } else if (guestSignal >= 0) {
    ORBIS_LOG_WARNING(__FUNCTION__, "handled signal", guestSignal, inGuestCode,
                      ::getpid(), thread->tid);

    if (!rx::thread::invokeSignalHandler(thread, guestSignal,
                                         inGuestCode ? context : nullptr)) {
      // no handler, mark signal as delivered
      std::uint32_t prevValue = 1;
      if (thread->interruptedMtx.compare_exchange_strong(prevValue, 0)) {
        thread->interruptedMtx.notify_one();
      }

      guestSignal = -1;
    }
  }

  if (inGuestCode && guestSignal != -1) {
    std::uint32_t prevValue = 1;
    if (thread->interruptedMtx.compare_exchange_strong(prevValue, 0)) {
      thread->interruptedMtx.notify_one();
    }
  }

  if (inGuestCode) {
    _writefsbase_u64(thread->fsBase);
  }
}

std::size_t rx::thread::getSigAltStackSize() {
  static auto sigStackSize = std::max<std::size_t>(
      SIGSTKSZ, ::rx::alignUp(64 * 1024 * 1024, rx::mem::pageSize));
  return sigStackSize;
}

bool rx::thread::invokeSignalHandler(orbis::Thread *thread, int guestSignal,
                                     ucontext_t *context) {
  auto it = thread->tproc->sigActions.find(guestSignal);

  if (it == thread->tproc->sigActions.end()) {
    return false;
  }

  auto guestContext =
      reinterpret_cast<ucontext_t *>(context ? context : thread->context);

  auto sigact = it->second;
  thread->setSigMask(sigact.mask);
  auto handlerPtr = reinterpret_cast<std::uintptr_t>(sigact.handler);

  auto rsp = guestContext->uc_mcontext.gregs[REG_RSP];

  ORBIS_LOG_WARNING(__FUNCTION__, "invoking signal handler", guestSignal, rsp,
                    thread->stackStart, thread->stackEnd);

  // FIXME: alt stack?
  rsp -= 128; // redzone
  rsp -= sizeof(orbis::SigFrame);

  // FIXME handle flags
  auto &sigFrame = *std::bit_cast<orbis::SigFrame *>(rsp);
  sigFrame = {};

  rx::thread::copyContext(thread, sigFrame.context, *guestContext);
  sigFrame.info.signo = guestSignal;
  sigFrame.handler = handlerPtr;

  guestContext->uc_mcontext.gregs[REG_RDI] = guestSignal; // arg1, signo
  guestContext->uc_mcontext.gregs[REG_RSI] =
      std::bit_cast<std::uintptr_t>(&sigFrame.info); // arg2, siginfo
  guestContext->uc_mcontext.gregs[REG_RDX] =
      std::bit_cast<std::uintptr_t>(&sigFrame.context); // arg3, ucontext
  guestContext->uc_mcontext.gregs[REG_RCX] = 0;         // arg4, si_addr
  guestContext->uc_mcontext.gregs[REG_RIP] = handlerPtr;

  guestContext->uc_mcontext.gregs[REG_RSP] = rx::alignDown(rsp, 16);
  return true;
}

void rx::thread::copyContext(orbis::MContext &dst, const mcontext_t &src) {
  // dst.onstack = src.gregs[REG_ONSTACK];
  dst.rdi = src.gregs[REG_RDI];
  dst.rsi = src.gregs[REG_RSI];
  dst.rdx = src.gregs[REG_RDX];
  dst.rcx = src.gregs[REG_RCX];
  dst.r8 = src.gregs[REG_R8];
  dst.r9 = src.gregs[REG_R9];
  dst.rax = src.gregs[REG_RAX];
  dst.rbx = src.gregs[REG_RBX];
  dst.rbp = src.gregs[REG_RBP];
  dst.r10 = src.gregs[REG_R10];
  dst.r11 = src.gregs[REG_R11];
  dst.r12 = src.gregs[REG_R12];
  dst.r13 = src.gregs[REG_R13];
  dst.r14 = src.gregs[REG_R14];
  dst.r15 = src.gregs[REG_R15];
  dst.trapno = src.gregs[REG_TRAPNO];
  dst.fs = src.gregs[REG_CSGSFS] & 0xffff;
  dst.gs = (src.gregs[REG_CSGSFS] >> 16) & 0xffff;
  // dst.addr = src.gregs[REG_ADDR];
  // dst.flags = src.gregs[REG_FLAGS];
  // dst.es = src.gregs[REG_ES];
  // dst.ds = src.gregs[REG_DS];
  dst.err = src.gregs[REG_ERR];
  dst.rip = src.gregs[REG_RIP];
  dst.cs = (src.gregs[REG_CSGSFS] >> 32) & 0xffff;
  dst.rflags = src.gregs[REG_EFL];
  dst.rsp = src.gregs[REG_RSP];
  // dst.ss = src.gregs[REG_SS];
  dst.len = sizeof(orbis::MContext);
  // dst.fpformat = src.gregs[REG_FPFORMAT];
  // dst.ownedfp = src.gregs[REG_OWNEDFP];
  // dst.lbrfrom = src.gregs[REG_LBRFROM];
  // dst.lbrto = src.gregs[REG_LBRTO];
  // dst.aux1 = src.gregs[REG_AUX1];
  // dst.aux2 = src.gregs[REG_AUX2];
  // dst.fpstate = src.gregs[REG_FPSTATE];
  // dst.fsbase = src.gregs[REG_FSBASE];
  // dst.gsbase = src.gregs[REG_GSBASE];
  // dst.xfpustate = src.gregs[REG_XFPUSTATE];
  // dst.xfpustate_len = src.gregs[REG_XFPUSTATE_LEN];
}

void rx::thread::copyContext(orbis::Thread *thread, orbis::UContext &dst,
                             const ucontext_t &src) {
  dst = {};
  dst.stack.sp = thread->stackStart;
  dst.stack.size = (char *)thread->stackEnd - (char *)thread->stackStart;
  dst.stack.align = 16;
  dst.sigmask = thread->sigMask;
  copyContext(dst.mcontext, src.uc_mcontext);
}

void rx::thread::setContext(orbis::Thread *thread, const orbis::UContext &src) {
  auto &context = *std::bit_cast<ucontext_t *>(thread->context);
  thread->stackStart = src.stack.sp;
  thread->stackEnd = (char *)thread->stackStart + src.stack.size;
  thread->setSigMask(src.sigmask);

  // dst.onstack = src.gregs[REG_ONSTACK];
  context.uc_mcontext.gregs[REG_RDI] = src.mcontext.rdi;
  context.uc_mcontext.gregs[REG_RSI] = src.mcontext.rsi;
  context.uc_mcontext.gregs[REG_RDX] = src.mcontext.rdx;
  context.uc_mcontext.gregs[REG_RCX] = src.mcontext.rcx;
  context.uc_mcontext.gregs[REG_R8] = src.mcontext.r8;
  context.uc_mcontext.gregs[REG_R9] = src.mcontext.r9;
  context.uc_mcontext.gregs[REG_RAX] = src.mcontext.rax;
  context.uc_mcontext.gregs[REG_RBX] = src.mcontext.rbx;
  context.uc_mcontext.gregs[REG_RBP] = src.mcontext.rbp;
  context.uc_mcontext.gregs[REG_R10] = src.mcontext.r10;
  context.uc_mcontext.gregs[REG_R11] = src.mcontext.r11;
  context.uc_mcontext.gregs[REG_R12] = src.mcontext.r12;
  context.uc_mcontext.gregs[REG_R13] = src.mcontext.r13;
  context.uc_mcontext.gregs[REG_R14] = src.mcontext.r14;
  context.uc_mcontext.gregs[REG_R15] = src.mcontext.r15;

  context.uc_mcontext.gregs[REG_TRAPNO] = src.mcontext.trapno;

  // in perfect world:
  // std::uint64_t csgsfs = 0;
  // csgsfs |= src.mcontext.fs;
  // csgsfs |= static_cast<std::uint64_t>(src.mcontext.gs) << 16;
  // csgsfs |= static_cast<std::uint64_t>(src.mcontext.cs) << 32;
  // context.uc_mcontext.gregs[REG_CSGSFS] = csgsfs;

  context.uc_mcontext.gregs[REG_CSGSFS] &= ~0xff'ffull;
  context.uc_mcontext.gregs[REG_CSGSFS] |= src.mcontext.fs;

  // dst.addr = src.gregs[REG_ADDR];
  // dst.flags = src.gregs[REG_FLAGS];
  // dst.es = src.gregs[REG_ES];
  // dst.ds = src.gregs[REG_DS];
  context.uc_mcontext.gregs[REG_ERR] = src.mcontext.err;
  context.uc_mcontext.gregs[REG_RIP] = src.mcontext.rip;
  context.uc_mcontext.gregs[REG_EFL] = src.mcontext.rflags;
  context.uc_mcontext.gregs[REG_RSP] = src.mcontext.rsp;
  // dst.ss = src.gregs[REG_SS];
  // dst.len = sizeof(orbis::MContext);
  // dst.fpformat = src.gregs[REG_FPFORMAT];
  // dst.ownedfp = src.gregs[REG_OWNEDFP];
  // dst.lbrfrom = src.gregs[REG_LBRFROM];
  // dst.lbrto = src.gregs[REG_LBRTO];
  // dst.aux1 = src.gregs[REG_AUX1];
  // dst.aux2 = src.gregs[REG_AUX2];
  // dst.fpstate = src.gregs[REG_FPSTATE];
  // dst.fsbase = src.gregs[REG_FSBASE];
  // dst.gsbase = src.gregs[REG_GSBASE];
  // dst.xfpustate = src.gregs[REG_XFPUSTATE];
  // dst.xfpustate_len = src.gregs[REG_XFPUSTATE_LEN];
}

void rx::thread::initialize() {
  struct sigaction act{};
  act.sa_sigaction = handleSigSys;
  act.sa_flags = SA_SIGINFO | SA_ONSTACK;

  sigaddset(&act.sa_mask, SIGSYS);
  sigaddset(&act.sa_mask, SIGUSR1);

  if (sigaction(SIGSYS, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }

  act.sa_sigaction = handleSigUser;

  if (sigaction(SIGUSR1, &act, NULL)) {
    perror("Error sigaction:");
    exit(-1);
  }
}

void rx::thread::deinitialize() {}

void *rx::thread::setupSignalStack(void *address) {
  stack_t ss{}, oss{};

  if (address == NULL) {
    std::fprintf(stderr, "attempt to set null signal stack, %p - %zx\n",
                 address, getSigAltStackSize());
    std::exit(EXIT_FAILURE);
  }

  ss.ss_sp = address;
  ss.ss_size = getSigAltStackSize();
  ss.ss_flags = 1 << 31;

  if (sigaltstack(&ss, &oss) == -1) {
    perror("sigaltstack");
    std::exit(EXIT_FAILURE);
  }

  return oss.ss_sp;
}

void *rx::thread::setupSignalStack() {
  auto data = malloc(getSigAltStackSize());
  if (data == nullptr) {
    rx::println(stderr, "malloc produces null, {:x}", getSigAltStackSize());
    std::exit(EXIT_FAILURE);
  }
  return setupSignalStack(data);
}

void rx::thread::setupThisThread() {
  sigset_t unblockSigs{};
  sigset_t oldSigmask{};
  sigaddset(&unblockSigs, SIGSYS);
  sigaddset(&unblockSigs, SIGUSR1);
  if (pthread_sigmask(SIG_UNBLOCK, &unblockSigs, &oldSigmask)) {
    perror("pthread_sigmask failed\n");
    std::exit(-1);
  }

  if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
            (void *)orbis::kMaxAddress, ~0ull - orbis::kMaxAddress, nullptr)) {
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

  ::setContext(context->uc_mcontext);
  _writefsbase_u64(hostFs);
}
