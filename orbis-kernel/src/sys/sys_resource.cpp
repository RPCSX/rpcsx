#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"

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
  if (function == 0) {
    rtp->type = 2;
    rtp->prio = 10;
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
  return ErrorCode::NOSYS;
}
