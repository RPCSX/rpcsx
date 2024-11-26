#include "KernelContext.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/Thread.hpp"
#include "thread/cpuset.hpp"
#include "utils/Logs.hpp"

#include <bit>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>

enum class CpuLevel {
  Root = 1,
  CpuSet = 2,
  Which = 3,
};

enum class CpuWhich {
  Tid = 1,
  Pid = 2,
  CpuSet = 3,
  Irq = 4,
  Jail = 5,
};

static cpu_set_t toHostCpuSet(orbis::cpuset cpuSet) {
  const int procCount = get_nprocs();
  cpu_set_t result{};

  for (unsigned cpu = std::countr_zero(cpuSet.bits);
       cpu < sizeof(cpuSet.bits) * 8;
       cpu = std::countr_zero(cpuSet.bits >> (cpu + 1)) + cpu + 1) {
    unsigned hostCpu = cpu;
    if (procCount < 8) {
      hostCpu = cpu % procCount;
    } else if (procCount >= 8 * 2) {
      hostCpu = cpu * 2;
    }

    ORBIS_LOG_ERROR(__FUNCTION__, cpu, hostCpu);
    CPU_SET(hostCpu, &result);
  }

  ORBIS_LOG_ERROR(__FUNCTION__, procCount, result.__bits[0], cpuSet.bits);
  return result;
}

orbis::SysResult orbis::sys_cpuset(Thread *thread, ptr<cpusetid_t> setid) {
  return {};
}
orbis::SysResult orbis::sys_cpuset_setid(Thread *thread, cpuwhich_t which,
                                         id_t id, cpusetid_t setid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cpuset_getid(Thread *thread, cpulevel_t level,
                                         cpuwhich_t which, id_t id,
                                         ptr<cpusetid_t> setid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cpuset_getaffinity(Thread *thread, cpulevel_t level,
                                               cpuwhich_t which, id_t id,
                                               size_t cpusetsize,
                                               ptr<cpuset> mask) {
  if (cpusetsize < sizeof(cpuset)) {
    return ErrorCode::INVAL;
  }

  std::lock_guard lock(thread->mtx);
  std::lock_guard lockProc(thread->tproc->mtx);

  switch (CpuLevel{level}) {
  case CpuLevel::Root:
  case CpuLevel::CpuSet:
    ORBIS_LOG_ERROR(__FUNCTION__, level, which, id, cpusetsize);
    return ErrorCode::INVAL;

  case CpuLevel::Which:
    switch (CpuWhich(which)) {
    case CpuWhich::Tid: {
      Thread *whichThread = nullptr;
      if (id == ~id_t(0) || thread->tid == id) {
        whichThread = thread;
      } else {
        whichThread = thread->tproc->threadsMap.get(id - thread->tproc->pid);
        if (whichThread == nullptr) {
          ORBIS_LOG_ERROR(__FUNCTION__, "thread not found", level, which, id,
                          cpusetsize);
          return ErrorCode::SRCH;
        }
      }

      return uwrite(mask, whichThread->affinity);
    }
    case CpuWhich::Pid: {
      Process *whichProcess = nullptr;
      if (id == ~id_t(0) || id == thread->tproc->pid) {
        whichProcess = thread->tproc;
      } else {
        whichProcess = g_context.findProcessById(id);

        if (whichProcess == nullptr) {
          return ErrorCode::SRCH;
        }
      }

      return uwrite(mask, whichProcess->affinity);
    }
    case CpuWhich::CpuSet:
    case CpuWhich::Irq:
    case CpuWhich::Jail:
      ORBIS_LOG_ERROR(__FUNCTION__, level, which, id, cpusetsize);
      return ErrorCode::INVAL;
    }
    break;
  }

  return ErrorCode::INVAL;
}
orbis::SysResult orbis::sys_cpuset_setaffinity(Thread *thread, cpulevel_t level,
                                               cpuwhich_t which, id_t id,
                                               size_t cpusetsize,
                                               ptr<const cpuset> mask) {
  std::lock_guard lock(thread->mtx);
  std::lock_guard lockProc(thread->tproc->mtx);
  switch (CpuLevel{level}) {
  case CpuLevel::Root:
  case CpuLevel::CpuSet:
    ORBIS_LOG_ERROR(__FUNCTION__, level, which, id, cpusetsize);
    return ErrorCode::INVAL;

  case CpuLevel::Which:
    switch (CpuWhich(which)) {
    case CpuWhich::Tid: {
      Thread *whichThread = nullptr;
      if (id == ~id_t(0) || thread->tid == id) {
        whichThread = thread;
      } else {
        whichThread = thread->tproc->threadsMap.get(id - thread->tproc->pid);
        if (whichThread == nullptr) {
          ORBIS_LOG_ERROR(__FUNCTION__, "thread not found", level, which, id,
                          cpusetsize);
          return ErrorCode::SRCH;
        }
      }

      ORBIS_RET_ON_ERROR(uread(whichThread->affinity, mask));
      auto threadHandle = whichThread->getNativeHandle();
      auto hostCpuSet = toHostCpuSet(whichThread->affinity);
      ORBIS_LOG_ERROR(__FUNCTION__, threadHandle, thread->tid, id);
      if (pthread_setaffinity_np(threadHandle, sizeof(hostCpuSet), &hostCpuSet)) {
        ORBIS_LOG_ERROR(__FUNCTION__,
                        "failed to set affinity mask for host thread",
                        whichThread->hostTid, whichThread->affinity.bits);
      }
      return {};
    }
    case CpuWhich::Pid: {
      Process *whichProcess = nullptr;
      if (id == ~id_t(0) || id == thread->tproc->pid) {
        whichProcess = thread->tproc;
      } else {
        ORBIS_LOG_ERROR(__FUNCTION__, "process not found", level, which, id,
                        cpusetsize);
        whichProcess = g_context.findProcessById(id);

        if (whichProcess == nullptr) {
          return ErrorCode::SRCH;
        }
      }

      ORBIS_RET_ON_ERROR(uread(whichProcess->affinity, mask));
      auto hostCpuSet = toHostCpuSet(whichProcess->affinity);

      if (sched_setaffinity(whichProcess->hostPid, sizeof(hostCpuSet),
                            &hostCpuSet)) {
        ORBIS_LOG_ERROR(__FUNCTION__,
                        "failed to set affinity mask for host process",
                        whichProcess->hostPid, whichProcess->affinity.bits);
      }

      return {};
    }
    case CpuWhich::CpuSet:
    case CpuWhich::Irq:
    case CpuWhich::Jail:
      ORBIS_LOG_ERROR(__FUNCTION__, level, which, id, cpusetsize);
      return ErrorCode::INVAL;
    }
    break;
  }

  return ErrorCode::INVAL;
}
