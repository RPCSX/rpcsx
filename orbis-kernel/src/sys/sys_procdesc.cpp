#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_pdgetpid(Thread *thread, sint fd, ptr<pid_t> pidp) { return ErrorCode::NOSYS; }
