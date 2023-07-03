#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_execve(Thread *thread, ptr<char> fname, ptr<ptr<char>> argv, ptr<ptr<char>> envv) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_fexecve(Thread *thread, sint fd, ptr<ptr<char>> argv, ptr<ptr<char>> envv) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_execve(Thread *thread, ptr<char> fname, ptr<ptr<char>> argv, ptr<ptr<char>> envv, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
