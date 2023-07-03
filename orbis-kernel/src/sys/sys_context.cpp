#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_getcontext(Thread *thread, ptr<struct ucontext> ucp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_setcontext(Thread *thread, ptr<struct ucontext> ucp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_swapcontext(Thread *thread, ptr<struct ucontext> oucp, ptr<struct ucontext> ucp) { return ErrorCode::NOSYS; }
