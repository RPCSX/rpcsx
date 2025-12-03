#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_cap_enter(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_getmode(Thread *thread, ptr<uint> modep) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_new(Thread *thread, FileDescriptor fd,
                                    uint64_t rights) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_getrights(Thread *thread, FileDescriptor fd,
                                          ptr<uint64_t> rights) {
  return ErrorCode::NOSYS;
}
