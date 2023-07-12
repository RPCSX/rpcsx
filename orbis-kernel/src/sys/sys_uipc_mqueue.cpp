#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_kmq_open(Thread *thread, ptr<const char> path,
                                     sint flags, mode_t mode,
                                     ptr<const struct mq_attr> attr) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kmq_unlink(Thread *thread, ptr<const char> path) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kmq_setattr(Thread *thread, sint mqd,
                                        ptr<const struct mq_attr> attr,
                                        ptr<struct mq_attr> oattr) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kmq_timedreceive(Thread *thread, sint mqd,
                                             ptr<const char> msg_ptr,
                                             size_t msg_len, ptr<uint> msg_prio,
                                             ptr<const timespec> abstimeout) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kmq_timedsend(Thread *thread, sint mqd,
                                          ptr<char> msg_ptr, size_t msg_len,
                                          ptr<uint> msg_prio,
                                          ptr<const timespec> abstimeout) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kmq_notify(Thread *thread, sint mqd,
                                       ptr<const struct sigevent> sigev) {
  return ErrorCode::NOSYS;
}
