#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/Thread.hpp"
#include "utils/Logs.hpp"
#include <sched.h>

namespace orbis {
struct rlimit {
  int64_t softLimit;
  int64_t hardLimit;
};
} // namespace orbis

orbis::SysResult orbis::sys_getpriority(Thread *thread, sint which, sint who) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setpriority(Thread *thread, sint which, sint who,
                                        sint prio) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_rtprio_thread(Thread *thread, sint function,
                                          lwpid_t lwpid,
                                          ptr<struct rtprio> rtp) {
  ORBIS_LOG_ERROR(__FUNCTION__, function, lwpid, rtp->prio, rtp->type);
  thread->where();

  Thread *targetThread;
  if (lwpid == thread->tid || lwpid == -1) {
    targetThread = thread;
  } else {
    targetThread = thread->tproc->threadsMap.get(lwpid - thread->tproc->pid);
    if (targetThread == nullptr) {
      return ErrorCode::SRCH;
    }
  }
  if (function == 0) {
    return orbis::uwrite(rtp, targetThread->prio);
  } else if (function == 1) {
    ORBIS_RET_ON_ERROR(orbis::uread(targetThread->prio, rtp));

    int hostPolicy = SCHED_RR;
    auto prioMin = sched_get_priority_min(hostPolicy);
    auto prioMax = sched_get_priority_max(hostPolicy);
    auto hostPriority =
        (targetThread->prio.prio * (prioMax - prioMin + 1)) / 1000 - prioMin;
    ::sched_param hostParam{};
    hostParam.sched_priority = hostPriority;
    if (pthread_setschedparam(targetThread->getNativeHandle(), hostPolicy,
                              &hostParam)) {

      auto normPrio = targetThread->prio.prio / 1000.f;
      hostParam.sched_priority = 0;

      if (normPrio < 0.3f) {
        hostPolicy = SCHED_BATCH;
      } else if (normPrio < 0.7f) {
        hostPolicy = SCHED_OTHER;
      } else {
        hostPolicy = SCHED_IDLE;
      }

      if (pthread_setschedparam(targetThread->getNativeHandle(),
                                hostPolicy, &hostParam)) {
        ORBIS_LOG_ERROR(__FUNCTION__, "failed to set host priority",
                        hostPriority, targetThread->prio.prio,
                        targetThread->prio.type, errno,
                        targetThread->getNativeHandle(), prioMin, prioMax, errno);
      }
    } else {
      ORBIS_LOG_ERROR(__FUNCTION__, "set host priority", hostPriority,
                      targetThread->tid, targetThread->prio.prio,
                      targetThread->prio.type, prioMin, prioMax);
    }
  }
  return {};
}
orbis::SysResult orbis::sys_rtprio(Thread *thread, sint function, pid_t pid,
                                   ptr<struct rtprio> rtp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setrlimit(Thread *thread, uint which,
                                      ptr<struct rlimit> rlp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getrlimit(Thread *thread, uint which,
                                      ptr<struct rlimit> rlp) {
  switch (which) {
  case 0: { // cpu
    break;
  }
  case 1: { // fsize
    break;
  }
  case 2: { // data
    break;
  }
  case 3: { // stack
    break;
  }
  case 4: { // core
    break;
  }
  case 5: { // rss
    break;
  }
  case 6: { // memlock
    break;
  }
  case 7: { // nproc
    break;
  }
  case 8: { // nofile
    break;
  }
  case 9: { // sbsize
    break;
  }
  case 10: { // vmem
    break;
  }
  case 11: { // npts
    break;
  }
  case 12: { // swap
    break;
  }

  default:
    return ErrorCode::INVAL;
  }

  rlp->softLimit = 4096;
  rlp->hardLimit = 4096;

  return {};
}
orbis::SysResult orbis::sys_getrusage(Thread *thread, sint who,
                                      ptr<struct rusage> rusage) {
  return {};
}
