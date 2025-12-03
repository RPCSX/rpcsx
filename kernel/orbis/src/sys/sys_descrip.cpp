#include "file.hpp"
#include "orbis/utils/Logs.hpp"
#include "stat.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/Thread.hpp"
#include <utility>

orbis::SysResult orbis::sys_getdtablesize(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dup2(Thread *thread, FileDescriptor from,
                                 FileDescriptor to) {
  auto file = thread->tproc->fileDescriptors.get(from);

  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  thread->tproc->fileDescriptors.close(to);
  thread->tproc->fileDescriptors.insert(to, file);
  return {};
}
orbis::SysResult orbis::sys_dup(Thread *thread, FileDescriptor fd) {
  auto file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }
  thread->retval[0] = std::to_underlying(
      thread->tproc->fileDescriptors.insert(std::move(file)));
  return {};
}
orbis::SysResult orbis::sys_fcntl(Thread *thread, FileDescriptor fd, sint cmd,
                                  slong arg) {
  ORBIS_LOG_TODO(__FUNCTION__, (int)fd, cmd, arg);
  return {};
}
orbis::SysResult orbis::sys_close(Thread *thread, FileDescriptor fd) {
  // ORBIS_LOG_NOTICE(__FUNCTION__, (int)fd);
  if (thread->tproc->fileDescriptors.close(fd)) {
    return {};
  }

  return ErrorCode::BADF;
}
orbis::SysResult orbis::sys_closefrom(Thread *thread, FileDescriptor lowfd) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fstat(Thread *thread, FileDescriptor fd,
                                  ptr<Stat> ub) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
orbis::SysResult orbis::sys_nfstat(Thread *thread, FileDescriptor fd,
                                   ptr<struct nstat> sb) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fpathconf(Thread *thread, FileDescriptor fd,
                                      sint name) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_flock(Thread *thread, FileDescriptor fd, sint how) {
  return ErrorCode::NOSYS;
}
