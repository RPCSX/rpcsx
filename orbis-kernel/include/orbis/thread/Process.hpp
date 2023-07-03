#pragma once
#include "ProcessState.hpp"
#include "orbis-config.hpp"
#include "orbis/module/Module.hpp"
#include "orbis/utils/IdMap.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "../thread/types.hpp"
#include "../thread/Thread.hpp"

#include <mutex>

namespace orbis {
class KernelContext;
struct Thread;
struct ProcessOps;
struct sysentvec;

struct Process {
  KernelContext *context = nullptr;
  pid_t pid = -1;
  sysentvec *sysent = nullptr;
  ProcessState state = ProcessState::NEW;
  Process *parentProcess = nullptr;
  shared_mutex mtx;
  void (*onSysEnter)(Thread *thread, int id, uint64_t *args, int argsCount) = nullptr;
  void (*onSysExit)(Thread *thread, int id, uint64_t *args, int argsCount, SysResult result) = nullptr;
  ptr<void> processParam = nullptr;
  uint64_t processParamSize = 0;
  const ProcessOps *ops = nullptr;

  std::uint64_t nextTlsSlot = 1;
  std::uint64_t lastTlsOffset = 0;

  utils::RcIdMap<Module, ModuleHandle> modulesMap;
  utils::OwningIdMap<Thread, lwpid_t> threadsMap;
  utils::RcIdMap<utils::RcBase, sint> fileDescriptors;
};
} // namespace orbis
