#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_exit(Thread *thread, sint status) {
  if (auto exit = thread->tproc->ops->exit) {
    return exit(thread, status);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_abort2(Thread *thread, ptr<const char> why, sint narg, ptr<ptr<void>> args) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_wait4(Thread *thread, sint pid, ptr<sint> status, sint options, ptr<struct rusage> rusage) { return ErrorCode::NOSYS; }


