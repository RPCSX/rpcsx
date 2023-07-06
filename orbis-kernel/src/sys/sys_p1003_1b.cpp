#include "sys/sysproto.hpp"

orbis::SysResult
orbis::sys_sched_setparam(Thread *thread, pid_t pid,
                          ptr<const struct sched_param> param) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sched_getparam(Thread *thread, pid_t pid,
                                           ptr<struct sched_param> param) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_sched_setscheduler(Thread *thread, pid_t pid, sint policy,
                              ptr<const struct sched_param> param) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sched_getscheduler(Thread *thread, pid_t pid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sched_yield(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sched_get_priority_max(Thread *thread,
                                                   sint policy) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sched_get_priority_min(Thread *thread,
                                                   sint policy) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_sched_rr_get_interval(Thread *thread, pid_t pid,
                                 ptr<struct timespec> interval) {
  return ErrorCode::NOSYS;
}
