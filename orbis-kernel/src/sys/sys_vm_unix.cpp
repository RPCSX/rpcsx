#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_obreak(Thread *thread, ptr<char> nsize) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ovadvise(Thread *thread, sint anom) {
  return ErrorCode::NOSYS;
}
