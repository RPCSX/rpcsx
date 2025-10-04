#include "KernelContext.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/ProcessOps.hpp"
#include "utils/Logs.hpp"
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

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

  int hostPid = pid;
  if (pid > 0) {
    auto process = g_context.findProcessById(pid);
    if (process == nullptr) {
      return ErrorCode::SRCH;
    }
    hostPid = process->hostPid;
  }

  ::rusage hostUsage;
  while (true) {
    int stat;
    int result = ::wait4(hostPid, &stat, options, &hostUsage);
    if (result < 0) {
      return static_cast<ErrorCode>(errno);
    }

    ORBIS_LOG_ERROR(__FUNCTION__, pid, status, options, rusage, result, stat);

    auto process = g_context.findProcessByHostId(result);
    if (process == nullptr) {
      ORBIS_LOG_ERROR("host process not found", result);
      continue;
    }

    if (status != nullptr) {
      ORBIS_RET_ON_ERROR(uwrite(status, stat));
    }
    thread->retval[0] = process->pid;
    break;
  }

  return {};
}
