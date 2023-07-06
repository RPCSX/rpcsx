#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_cap_enter(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_getmode(Thread *thread, ptr<uint> modep) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_new(Thread *thread, sint fd, uint64_t rights) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_getrights(Thread *thread, sint fd,
                                          ptr<uint64_t> rights) {
  return ErrorCode::NOSYS;
}
