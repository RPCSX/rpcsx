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
  ORBIS_LOG_WARNING(__FUNCTION__, fd, ub);
  thread->where();
  if (fd == 0) {
    return ErrorCode::PERM;
  }

  auto result = sys_lseek(thread, fd, 0, SEEK_CUR);
  auto oldpos = thread->retval[0];
  if (result.isError()) {
    return result;
  }

  result = sys_lseek(thread, fd, 0, SEEK_END);
  auto len = thread->retval[0];
  if (result.isError()) {
    return result;
  }

  result = sys_read(thread, fd, ub, 1);
  if (result.isError()) {
    *ub = {};
    ub->mode = 0777 | 0x4000;
  } else {
    *ub = {};
    ub->size = len;
    ub->blksize = 1;
    ub->blocks = len;
    ub->mode = 0777 | 0x8000;
  }

  result = sys_lseek(thread, fd, oldpos, SEEK_SET);
  if (result.isError()) {
    return result;
  }

  thread->retval[0] = 0;
  return {};
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
