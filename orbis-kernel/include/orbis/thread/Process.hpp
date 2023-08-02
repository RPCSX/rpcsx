#pragma once
#include "orbis-config.hpp"

#include "../evf.hpp"
#include "../ipmi.hpp"
#include "../osem.hpp"
#include "../thread/Thread.hpp"
#include "../thread/types.hpp"
#include "ProcessState.hpp"
#include "orbis/file.hpp"
#include "orbis/module/Module.hpp"
#include "orbis/utils/IdMap.hpp"
#include "orbis/utils/SharedMutex.hpp"

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
  sysentvec *sysent = nullptr;
  ProcessState state = ProcessState::NEW;
  Process *parentProcess = nullptr;
  shared_mutex mtx;
  void (*onSysEnter)(Thread *thread, int id, uint64_t *args,
                     int argsCount) = nullptr;
  void (*onSysExit)(Thread *thread, int id, uint64_t *args, int argsCount,
                    SysResult result) = nullptr;
  ptr<void> processParam = nullptr;
  uint64_t processParamSize = 0;
  const ProcessOps *ops = nullptr;

  std::uint64_t nextTlsSlot = 1;
  std::uint64_t lastTlsOffset = 0;

  utils::RcIdMap<EventFlag, sint, 4097, 1> evfMap;
  utils::RcIdMap<Semaphore, sint, 4097, 1> semMap;
  utils::RcIdMap<IpmiClient, sint, 4097, 1> ipmiClientMap;
  utils::RcIdMap<Module, ModuleHandle> modulesMap;
  utils::OwningIdMap<Thread, lwpid_t> threadsMap;
  utils::RcIdMap<orbis::File, sint> fileDescriptors;

  // Named objects for debugging
  utils::shared_mutex namedObjMutex;
  utils::kmap<void *, utils::kstring> namedObjNames;
  utils::OwningIdMap<NamedObjInfo, uint, 65535, 1> namedObjIds;

  // Named memory ranges for debugging
  utils::shared_mutex namedMemMutex;
  utils::kmap<NamedMemoryRange, utils::kstring> namedMem;
};
} // namespace orbis
