#include "sys/sysproto.hpp"
#include "thread/Thread.hpp"
#include "utils/Logs.hpp"
#include <thread>

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
  std::this_thread::yield();
  return {};
}
orbis::SysResult orbis::sys_sched_get_priority_max(Thread *thread,
                                                   sint policy) {
  ORBIS_LOG_ERROR(__FUNCTION__, policy);
  thread->retval[0] = 90;
  return {};
}
orbis::SysResult orbis::sys_sched_get_priority_min(Thread *thread,
                                                   sint policy) {
  ORBIS_LOG_ERROR(__FUNCTION__, policy);
  thread->retval[0] = 10;
  return {};
}
orbis::SysResult orbis::sys_sched_rr_get_interval(Thread *thread, pid_t pid,
                                                  ptr<timespec> interval) {
  return ErrorCode::NOSYS;
}
