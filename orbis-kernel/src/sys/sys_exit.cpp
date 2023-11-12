#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"
#include <chrono>
#include <thread>

orbis::SysResult orbis::sys_exit(Thread *thread, sint status) {
  if (auto exit = thread->tproc->ops->exit) {
    return exit(thread, status);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_abort2(Thread *thread, ptr<const char> why,
                                   sint narg, ptr<ptr<void>> args) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_wait4(Thread *thread, sint pid, ptr<sint> status,
                                  sint options, ptr<struct rusage> rusage) {
  // TODO
  ORBIS_LOG_ERROR(__FUNCTION__, pid, status, options, rusage);
  std::this_thread::sleep_for(std::chrono::days(1));
  return {};
}
