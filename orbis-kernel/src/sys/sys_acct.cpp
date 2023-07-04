#include "sys/sysproto.hpp"
#include "orbis/error.hpp"

orbis::SysResult orbis::sys_acct(Thread *thread, ptr<char> path) { return ErrorCode::NOSYS; }
