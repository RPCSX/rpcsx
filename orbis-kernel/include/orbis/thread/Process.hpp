#pragma once
#include "orbis-config.hpp"

#include "../event.hpp"
#include "../evf.hpp"
#include "../ipmi.hpp"
#include "../osem.hpp"
#include "../thread/Thread.hpp"
#include "../thread/types.hpp"
#include "ProcessState.hpp"
#include "cpuset.hpp"
#include "orbis/AppInfo.hpp"
#include "orbis/AuthInfo.hpp"
#include "orbis/file.hpp"
#include "orbis/module/Module.hpp"
#include "orbis/utils/IdMap.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include <optional>

namespace orbis {
class KernelContext;
struct Thread;
struct ProcessOps;
struct sysentvec;

struct NamedObjInfo {
  void *idptr;
  uint16_t ty;
};

struct NamedMemoryRange {
  uint64_t begin, end;

  constexpr bool operator<(const NamedMemoryRange &rhs) const {
    return end <= rhs.begin;
  }

  friend constexpr bool operator<(const NamedMemoryRange &lhs, uint64_t ptr) {
    return lhs.end <= ptr;
  }

  friend constexpr bool operator<(uint64_t ptr, const NamedMemoryRange &rhs) {
    return ptr < rhs.begin;
  }
};

struct Process final {
  KernelContext *context = nullptr;
  pid_t pid = -1;
  int gfxRing = 0;
  std::uint64_t hostPid = -1;
  sysentvec *sysent = nullptr;
  ProcessState state = ProcessState::NEW;
  Process *parentProcess = nullptr;
  shared_mutex mtx;
  int vmId = -1;
  void (*onSysEnter)(Thread *thread, int id, uint64_t *args,
                     int argsCount) = nullptr;
  void (*onSysExit)(Thread *thread, int id, uint64_t *args, int argsCount,
                    SysResult result) = nullptr;
  ptr<void> processParam = nullptr;
  uint64_t processParamSize = 0;
  const ProcessOps *ops = nullptr;
  AppInfo appInfo{};
  AuthInfo authInfo{};
  kstring cwd;
  kstring root = "/";
  cpuset affinity{(1 << 7) - 1};
  sint memoryContainer{1};
  sint budgetId{1};
  bool isInSandbox = false;
  EventEmitter event;
  std::optional<sint> exitStatus;

  std::uint32_t sdkVersion = 0;
  std::uint64_t nextTlsSlot = 1;
  std::uint64_t lastTlsOffset = 0;

  utils::RcIdMap<EventFlag, sint, 4097, 1> evfMap;
  utils::RcIdMap<Semaphore, sint, 4097, 1> semMap;
  utils::RcIdMap<Module, ModuleHandle> modulesMap;
  utils::OwningIdMap<Thread, lwpid_t> threadsMap;
  utils::RcIdMap<orbis::File, sint> fileDescriptors;

  // Named objects for debugging
  utils::shared_mutex namedObjMutex;
  utils::kmap<void *, utils::kstring> namedObjNames;
  utils::OwningIdMap<NamedObjInfo, uint, 65535, 1> namedObjIds;

  utils::kmap<std::int32_t, SigAction> sigActions;

  // Named memory ranges for debugging
  utils::shared_mutex namedMemMutex;
  utils::kmap<NamedMemoryRange, utils::kstring> namedMem;
};
} // namespace orbis
