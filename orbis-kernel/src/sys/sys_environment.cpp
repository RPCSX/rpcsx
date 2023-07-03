#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_kenv(Thread *thread, sint what, ptr<const char> name, ptr<char> value, sint len) { return ErrorCode::NOSYS; }
