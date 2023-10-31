#include "KernelContext.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"
#include <cstdlib>
#include <unistd.h>

orbis::SysResult orbis::sys_fork(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_pdfork(Thread *thread, ptr<sint> fdp, sint flags) {
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_vfork(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rfork(Thread *thread, sint flags) {
  ORBIS_LOG_TODO(__FUNCTION__, flags);

  int hostPid = ::fork();
  if (hostPid) {
    thread->retval[0] = 10001;
    thread->retval[1] = 0;
  } else {
    auto process = g_context.createProcess(10001);
    std::lock_guard lock(thread->tproc->fileDescriptors.mutex);
    process->sysent = thread->tproc->sysent;
    process->onSysEnter = thread->tproc->onSysEnter;
    process->onSysExit = thread->tproc->onSysExit;
    process->ops = thread->tproc->ops;
    process->isSystem = thread->tproc->isSystem;
    for (auto [id, mod] : thread->tproc->modulesMap) {
      if (!process->modulesMap.insert(id, mod)) {
        std::abort();
      }
    }

    for (auto [id, mod] : thread->tproc->fileDescriptors) {
      if (!process->fileDescriptors.insert(id, mod)) {
        std::abort();
      }
    }

    auto [baseId, newThread] = process->threadsMap.emplace();
    newThread->tproc = process;
    newThread->tid = process->pid + baseId;
    newThread->state = orbis::ThreadState::RUNNING;
    newThread->context = thread->context;
    newThread->fsBase = thread->fsBase;

    orbis::g_currentThread = newThread;
    newThread->retval[0] = 0;
    newThread->retval[1] = 1;
  }
  return {};
}
