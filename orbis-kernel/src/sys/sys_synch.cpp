#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_yield(Thread *thread) { return ErrorCode::NOSYS; }
