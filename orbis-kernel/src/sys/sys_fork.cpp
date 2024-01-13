#include "KernelContext.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/ProcessOps.hpp"
#include <cstdlib>
#include <unistd.h>

orbis::SysResult orbis::sys_fork(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_pdfork(Thread *thread, ptr<sint> fdp, sint flags) {
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_vfork(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rfork(Thread *thread, sint flags) {
  if (auto fork = thread->tproc->ops->fork) {
    return fork(thread, flags);
  }
  return ErrorCode::NOSYS;
}
