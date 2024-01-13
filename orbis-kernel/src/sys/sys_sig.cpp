#include "KernelContext.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/Thread.hpp"
#include "ucontext.hpp"
#include "utils/Logs.hpp"
#include <csignal>

orbis::SysResult orbis::sys_sigaction(Thread *thread, sint sig,
                                      ptr<SigAction> act, ptr<SigAction> oact) {
  ORBIS_LOG_WARNING(__FUNCTION__, sig, act, oact);

  auto &sigAct = thread->tproc->sigActions[sig];
  if (oact != nullptr) {
    if (auto errc = uwrite(oact, sigAct); errc != orbis::ErrorCode{}) {
      return errc;
    }
  }

  if (act != nullptr) {
    if (auto errc = uread(sigAct, act); errc != ErrorCode{}) {
      return errc;
    }

    ORBIS_LOG_WARNING(__FUNCTION__, sigAct.handler, sigAct.flags,
                      sigAct.mask.bits[0], sigAct.mask.bits[1],
                      sigAct.mask.bits[2], sigAct.mask.bits[3]);
  }

  return {};
}

orbis::SysResult orbis::sys_sigprocmask(Thread *thread, sint how,
                                        ptr<SigSet> set, ptr<SigSet> oset) {
  if (oset) {
    ORBIS_RET_ON_ERROR(uwrite(oset, thread->sigMask));
  }

  if (set) {
    SigSet _set;
    ORBIS_RET_ON_ERROR(uread(_set, set));

    switch (how) {
    case 1: // block
      for (std::size_t i = 0; i < 4; ++i) {
        thread->sigMask.bits[i] |= _set.bits[i];
      }
      break;

    case 2: // unblock
      for (std::size_t i = 0; i < 4; ++i) {
        thread->sigMask.bits[i] &= ~_set.bits[i];
      }
      break;
    case 3: // set
      thread->sigMask = _set;
      break;

    default:
      ORBIS_LOG_ERROR("sys_sigprocmask: unimplemented how", how);
      thread->where();
      return ErrorCode::INVAL;
    }

    thread->sigMask.clear(kSigKill);
    thread->sigMask.clear(kSigStop);
  }

  return {};
}
orbis::SysResult orbis::sys_sigwait(Thread *thread, ptr<const SigSet> set,
                                    ptr<sint> sig) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigtimedwait(Thread *thread, ptr<const SigSet> set,
                                         ptr<struct siginfo> info,
                                         ptr<const timespec> timeout) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigwaitinfo(Thread *thread, ptr<const SigSet> set,
                                        ptr<struct siginfo> info) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigpending(Thread *thread, ptr<SigSet> set) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigsuspend(Thread *thread, ptr<const SigSet> set) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigaltstack(Thread *thread, ptr<struct stack_t> ss,
                                        ptr<struct stack_t> oss) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kill(Thread *thread, sint pid, sint signum) {
  ORBIS_LOG_WARNING(__FUNCTION__, pid, signum);

  int hostPid = pid;
  if (pid > 0) {
    auto process = g_context.findProcessById(pid);
    if (process == nullptr) {
      return ErrorCode::SRCH;
    }
    hostPid = process->hostPid;
  }

  // TODO: wrap signal
  // int result = ::kill(hostPid, signum);
  // if (result < 0) {
  //   return static_cast<ErrorCode>(errno);
  // }

  return {};
}

orbis::SysResult orbis::sys_pdkill(Thread *thread, sint fd, sint signum) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigqueue(Thread *thread, pid_t pid, sint signum,
                                     ptr<void> value) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigreturn(Thread *thread, ptr<UContext> sigcntxp) {
  ORBIS_LOG_WARNING(__FUNCTION__, sigcntxp);

  // auto sigRet = thread->sigReturns.front();
  // thread->sigReturns.erase(thread->sigReturns.begin(), thread->sigReturns.begin() + 1);
  // writeRegister(thread->context, RegisterId::rip, sigRet.rip);
  // writeRegister(thread->context, RegisterId::rsp, sigRet.rsp);
  // ORBIS_LOG_ERROR(__FUNCTION__, sigRet.rip, sigRet.rsp);
  return {};
}

orbis::SysResult orbis::nosys(Thread *thread) {
  thread->sendSignal(kSigSys);
  return{};
}
