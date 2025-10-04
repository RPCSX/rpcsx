#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_setfib(Thread *thread, sint fib) {
  return ErrorCode::NOSYS;
}
