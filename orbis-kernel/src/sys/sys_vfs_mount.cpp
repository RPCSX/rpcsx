#include "sys/sysproto.hpp"
#include "thread/ProcessOps.hpp"
#include "thread/Thread.hpp"
#include "thread/Process.hpp"

orbis::SysResult orbis::sys_mount(Thread *thread, ptr<char> type,
                                  ptr<char> path, sint flags, caddr_t data) {
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_unmount(Thread *thread, ptr<char> path,
                                    sint flags) {
  if (auto unmount = thread->tproc->ops->unmount) {
    return unmount(thread, path, flags);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_nmount(Thread *thread, ptr<IoVec> iovp, uint iovcnt,
                                   sint flags) {
  if (auto nmount = thread->tproc->ops->nmount) {
    return nmount(thread, iovp, iovcnt, flags);
  }

  return ErrorCode::NOSYS;
}
