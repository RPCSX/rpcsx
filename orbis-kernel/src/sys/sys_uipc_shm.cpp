#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_shm_open(Thread *thread, ptr<const char> path,
                                     sint flags, mode_t mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_shm_unlink(Thread *thread, ptr<const char> path) {
  return ErrorCode::NOSYS;
}
