#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_sigaction(Thread *thread, sint sig,
                                      ptr<struct sigaction> act,
                                      ptr<struct sigaction> oact) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigprocmask(Thread *thread, sint how,
                                        ptr<uint64_t> set, ptr<uint64_t> oset) {
  if (oset) {
    for (std::size_t i = 0; i < 2; ++i) {
      oset[i] = thread->sigMask[i];
    }
  }

  if (set) {
    switch (how) {
    case 0: // unblock
      for (std::size_t i = 0; i < 2; ++i) {
        thread->sigMask[i] &= ~set[i];
      }
    case 1: // block
      for (std::size_t i = 0; i < 2; ++i) {
        thread->sigMask[i] |= set[i];
      }
      break;
    case 3: // set
      for (std::size_t i = 0; i < 2; ++i) {
        thread->sigMask[i] = set[i];
      }
      break;

    default:
      return ErrorCode::INVAL;
    }
  }
  return {};
}
orbis::SysResult orbis::sys_sigwait(Thread *thread,
                                    ptr<const struct sigset> set,
                                    ptr<sint> sig) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigtimedwait(Thread *thread,
                                         ptr<const struct sigset> set,
                                         ptr<struct siginfo> info,
                                         ptr<const timespec> timeout) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigwaitinfo(Thread *thread,
                                        ptr<const struct sigset> set,
                                        ptr<struct siginfo> info) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigpending(Thread *thread, ptr<struct sigset> set) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigsuspend(Thread *thread,
                                       ptr<const struct sigset> set) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigaltstack(Thread *thread, ptr<struct stack_t> ss,
                                        ptr<struct stack_t> oss) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kill(Thread *thread, sint pid, sint signum) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_pdkill(Thread *thread, sint fd, sint signum) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigqueue(Thread *thread, pid_t pid, sint signum,
                                     ptr<void> value) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sigreturn(Thread *thread,
                                      ptr<struct ucontext> sigcntxp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::nosys(Thread *thread) { return ErrorCode::NOSYS; }
