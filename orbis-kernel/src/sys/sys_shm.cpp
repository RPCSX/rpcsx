#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_shmdt(Thread *thread, ptr<const void> shmaddr) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_shmat(Thread *thread, sint shmid,
                                  ptr<const void> shmaddr, sint shmflg) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_shmctl(Thread *thread, sint shmid, sint cmd,
                                   ptr<struct shmid_ds> buf) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_shmget(Thread *thread, key_t key, size_t size,
                                   sint shmflg) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_shmsys(Thread *thread, sint which, sint a2, sint a3,
                                   sint a4) {
  return ErrorCode::NOSYS;
}
