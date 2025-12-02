#pragma once
#include "orbis-config.hpp"

#include "../KernelAllocator.hpp"
#include "../KernelObject.hpp"
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
#include "orbis/Budget.hpp"
#include "orbis/file.hpp"
#include "orbis/module/Module.hpp"
#include "rx/IdMap.hpp"
#include "rx/Serializer.hpp"
#include "rx/SharedMutex.hpp"
#include <optional>
#include <type_traits>

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

enum class ProcessType : std::uint8_t {
  FreeBsd,
  Ps4,
  Ps5,
};

struct Process final {
  using Storage =
      kernel::StaticKernelObjectStorage<OrbisNamespace,
                                        kernel::detail::ProcessScope>;

  ~Process();

  KernelContext *context = nullptr;
  std::byte *storage = nullptr;

  pid_t pid = -1;
  int gfxRing = 0;
  std::uint64_t hostPid = -1;
  sysentvec *sysent = nullptr;
  ProcessState state = ProcessState::NEW;
  Process *parentProcess = nullptr;
  rx::shared_mutex mtx;
  int vmId = -1;
  ProcessType type = ProcessType::FreeBsd;
  void (*onSysEnter)(Thread *thread, int id, uint64_t *args,
                     int argsCount) = nullptr;
  void (*onSysExit)(Thread *thread, int id, uint64_t *args, int argsCount,
                    SysResult result) = nullptr;
  ptr<void> processParam = nullptr;
  uint64_t processParamSize = 0;
  const ProcessOps *ops = nullptr;
  AppInfoEx appInfo{};
  AuthInfo authInfo{};
  kstring cwd;
  kstring root = "/";
  cpuset affinity{(1 << 7) - 1};
  sint memoryContainer{1};
  sint budgetId{};
  Budget::ProcessType budgetProcessType{};
  bool isInSandbox = false;
  EventEmitter event;
  std::optional<sint> exitStatus;

  std::uint32_t sdkVersion = 0;
  bool allowDmemAliasing = false;
  std::uint64_t nextTlsSlot = 1;
  std::uint64_t lastTlsOffset = 0;

  rx::RcIdMap<EventFlag, sint, 4097, 1> evfMap;
  rx::RcIdMap<Semaphore, sint, 4097, 1> semMap;
  rx::RcIdMap<Module, ModuleHandle> modulesMap;
  rx::RcIdMap<Thread, lwpid_t> threadsMap;
  rx::RcIdMap<orbis::File, sint> fileDescriptors;

  rx::AddressRange libkernelRange;

  // Named objects for debugging
  rx::shared_mutex namedObjMutex;
  kmap<void *, kstring> namedObjNames;
  rx::OwningIdMap<NamedObjInfo, uint, 65535, 1> namedObjIds;

  kmap<std::int32_t, SigAction> sigActions;

  // FIXME: implement process destruction
  void incRef() {}
  void decRef() {}

  void serialize(rx::Serializer &) const;
  void deserialize(rx::Deserializer &);

  template <rx::Serializable T>
  T *get(
      kernel::StaticObjectRef<OrbisNamespace, kernel::detail::ProcessScope, T>
          ref) {
    return ref.get(storage);
  }

  Budget *getBudget() const;

  template <typename Cb>
    requires(alignof(Cb) <= 8 && sizeof(Cb) <= 64) &&
            (std::is_same_v<std::invoke_result_t<Cb>, void> ||
             (alignof(std::invoke_result_t<Cb>) <= 8 &&
              sizeof(std::invoke_result_t<Cb>) <= 64))
  std::invoke_result_t<Cb> invoke(Cb &&fn) {
    auto constructObject = [](void *to, void *from) {
      new (to) Cb(std::move(*reinterpret_cast<Cb *>(from)));
    };

    auto destroyObject = [](void *object) {
      reinterpret_cast<Cb *>(object)->~Cb();
    };

    if constexpr (std::is_same_v<std::invoke_result_t<Cb>, void>) {
      invokeImpl(
          nullptr, nullptr, &fn, constructObject, destroyObject,
          [](void *, void *fnPtr) { (*reinterpret_cast<Cb *>(fnPtr))(); });
    } else {
      alignas(std::invoke_result_t<Cb>) char
          result[sizeof(std::invoke_result_t<Cb>)];
      invokeImpl(
          &result,
          [](void *to, void *from) {
            new (to) std::invoke_result_t<Cb>(
                std::move(*reinterpret_cast<std::invoke_result_t<Cb> *>(from)));
          },
          &fn, constructObject, destroyObject,
          [](void *result, void *fnPtr) {
            new (result)
                std::invoke_result_t<Cb>((*reinterpret_cast<Cb *>(fnPtr))());
          });
      return std::move(*reinterpret_cast<std::invoke_result_t<Cb> *>(result));
    }
  }

  void invokeAsync(void (*fn)());

private:
  void invokeImpl(void *returnValue, void (*copyResult)(void *to, void *from),
                  void *fnPtr, void (*constructObject)(void *to, void *from),
                  void (*destroyObject)(void *to),
                  void (*invokeImpl)(void *returnValue, void *fnPtr));
};

pid_t allocatePid();
Process *createProcess(Process *parentProcess = nullptr, pid_t pid = -1);
void deleteProcess(Process *proc);
Process *findProcessById(pid_t pid);
Process *findProcessByHostId(std::uint64_t pid);
} // namespace orbis
