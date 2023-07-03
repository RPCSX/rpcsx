#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_ptrace(Thread *thread, sint req, pid_t pid, caddr_t addr, sint data) { return ErrorCode::NOSYS; }
