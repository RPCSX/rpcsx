#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_clock_gettime(Thread *thread, clockid_t clock_id, ptr<struct timespec> tp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_clock_settime(Thread *thread, clockid_t clock_id, ptr<const struct timespec> tp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_clock_getres(Thread *thread, clockid_t clock_id, ptr<struct timespec> tp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_nanosleep(Thread *thread, ptr<const struct timespec> rqtp, ptr<struct timespec> rmtp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_gettimeofday(Thread *thread, ptr<struct timeval> tp, ptr<struct timezone> tzp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_settimeofday(Thread *thread, ptr<struct timeval> tp, ptr<struct timezone> tzp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getitimer(Thread *thread, uint which, ptr<struct itimerval> itv) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_setitimer(Thread *thread, uint which, ptr<struct itimerval> itv, ptr<struct itimerval> oitv) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ktimer_create(Thread *thread, clockid_t clock_id, ptr<struct sigevent> evp, ptr<sint> timerid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ktimer_delete(Thread *thread, sint timerid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ktimer_settime(Thread *thread, sint timerid, sint flags, ptr<const struct itimerspec> value, ptr<struct itimerspec> ovalue) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ktimer_gettime(Thread *thread, sint timerid, ptr<struct itimerspec> value) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ktimer_getoverrun(Thread *thread, sint timerid) { return ErrorCode::NOSYS; }
