#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_swapon(Thread *thread, ptr<char> name) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_swapoff(Thread *thread, ptr<const char> name) {
  return ErrorCode::NOSYS;
}
