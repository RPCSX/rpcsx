#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_getpriority(Thread *thread, sint which, sint who) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_setpriority(Thread *thread, sint which, sint who, sint prio) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rtprio_thread(Thread *thread, sint function, lwpid_t lwpid, ptr<struct rtprio> rtp) {
  std::printf("sys_rtprio_thread: unimplemented\n");
  return {};
}
orbis::SysResult orbis::sys_rtprio(Thread *thread, sint function, pid_t pid, ptr<struct rtprio> rtp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_setrlimit(Thread *thread, uint which, ptr<struct rlimit> rlp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getrlimit(Thread *thread, uint which, ptr<struct rlimit> rlp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getrusage(Thread *thread, sint who, ptr<struct rusage> rusage) { return ErrorCode::NOSYS; }
