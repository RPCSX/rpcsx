#include "orbis/utils/Logs.hpp"
#include "stat.hpp"
#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_getdtablesize(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dup2(Thread *thread, uint from, uint to) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dup(Thread *thread, uint fd) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fcntl(Thread *thread, sint fd, sint cmd,
                                  slong arg) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_close(Thread *thread, sint fd) {
  ORBIS_LOG_NOTICE(__FUNCTION__, fd);
  if (auto close = thread->tproc->ops->close) {
    return close(thread, fd);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_closefrom(Thread *thread, sint lowfd) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fstat(Thread *thread, sint fd, ptr<Stat> ub) {
  ORBIS_LOG_TODO(__FUNCTION__, fd, ub);
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_nfstat(Thread *thread, sint fd,
                                   ptr<struct nstat> sb) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fpathconf(Thread *thread, sint fd, sint name) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_flock(Thread *thread, sint fd, sint how) {
  return ErrorCode::NOSYS;
}
