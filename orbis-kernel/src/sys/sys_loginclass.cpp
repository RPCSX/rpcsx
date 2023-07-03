#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_getloginclass(Thread *thread, ptr<char> namebuf, size_t namelen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_setloginclass(Thread *thread, ptr<char> namebuf) { return ErrorCode::NOSYS; }
