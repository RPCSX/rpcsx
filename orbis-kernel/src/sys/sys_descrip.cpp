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
  if (fd == 0) {
    return {}; // FIXME: remove when shm would be implemented
  }

  if (thread->tproc->fileDescriptors.close(fd)) {
    return {};
  }

  return ErrorCode::BADF;
}
orbis::SysResult orbis::sys_closefrom(Thread *thread, sint lowfd) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fstat(Thread *thread, sint fd, ptr<Stat> ub) {
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto stat = file->ops->stat;
  if (stat == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  Stat _ub;
  auto result = uread(_ub, ub);
  if (result != ErrorCode{}) {
    return result;
  }

  result = stat(file.get(), &_ub, thread);
  if (result != ErrorCode{}) {
    return result;
  }

  return uwrite(ub, _ub);
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
