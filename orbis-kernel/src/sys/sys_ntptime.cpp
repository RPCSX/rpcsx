#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_ntp_gettime(Thread *thread, ptr<struct ntptimeval> ntvp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ntp_adjtime(Thread *thread, ptr<struct timex> tp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_adjtime(Thread *thread, ptr<struct timeval> delta, ptr<struct timeval> olddelta) { return ErrorCode::NOSYS; }
