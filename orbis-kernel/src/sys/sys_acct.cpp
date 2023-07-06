#include "error.hpp"
#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_acct(Thread *thread, ptr<char> path) {
  return ErrorCode::NOSYS;
}
