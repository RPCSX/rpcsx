#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_fork(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_pdfork(Thread *thread, ptr<sint> fdp, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_vfork(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rfork(Thread *thread, sint flags) {
  return ErrorCode::NOSYS;
}
