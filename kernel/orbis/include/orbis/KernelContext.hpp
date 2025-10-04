#pragma once
#include "AppInfo.hpp"
#include "Budget.hpp"
#include "KernelAllocator.hpp"
#include "evf.hpp"
#include "ipmi.hpp"
#include "orbis/note.hpp"
#include "osem.hpp"
#include "rx/IdMap.hpp"
#include "rx/LinkedNode.hpp"
#include "rx/SharedCV.hpp"
#include "rx/SharedMutex.hpp"
#include "thread/types.hpp"

#include <cstdint>
#include <mutex>
#include <pthread.h>
#include <span>
#include <utility>

namespace orbis {
struct Process;
struct Thread;

struct UmtxKey {
  // TODO: may contain a reference to a shared memory
  std::uintptr_t addr;
  orbis::pid_t pid;

  auto operator<=>(const UmtxKey &) const = default;
};

struct UmtxCond {
  Thread *thr;
  rx::shared_cv cv;

  UmtxCond(Thread *thr) : thr(thr) {}
};

struct UmtxChain {
  rx::shared_mutex mtx;
  using queue_type = utils::kmultimap<UmtxKey, UmtxCond>;
  queue_type sleep_queue;
  queue_type spare_queue;

  std::pair<const UmtxKey, UmtxCond> *enqueue(UmtxKey &key, Thread *thr);
  void erase(std::pair<const UmtxKey, UmtxCond> *obj);
  queue_type::iterator erase(queue_type::iterator it);
  uint notify_one(const UmtxKey &key);
  uint notify_all(const UmtxKey &key);
  uint notify_n(const UmtxKey &key, sint count);
};

enum class FwType : std::uint8_t {
  Unknown,
  Ps4,
  Ps5,
};

struct RcAppInfo : rx::RcBase, AppInfoEx {
  orbis::uint32_t appState = 0;
};

class alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) KernelContext final {
public:
  KernelContext();
  ~KernelContext();

  Process *createProcess(pid_t pid);
  void deleteProcess(Process *proc);
  Process *findProcessById(pid_t pid) const;
  Process *findProcessByHostId(std::uint64_t pid) const;

  rx::LinkedNode<Process> *getProcessList() { return m_processes; }

  long allocatePid() {
    std::lock_guard lock(m_thread_id_mtx);
    return m_thread_id_map.emplace(0).first;
  }

  long getTscFreq();

  void *kalloc(std::size_t size,
               std::size_t align = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  void kfree(void *ptr, std::size_t size);

  std::pair<EventFlag *, bool> createEventFlag(utils::kstring name,
                                               std::int32_t flags,
                                               std::uint64_t initPattern) {
    std::lock_guard lock(m_evf_mtx);

    auto [it, inserted] = m_event_flags.try_emplace(std::move(name), nullptr);
    if (inserted) {
      it->second = knew<EventFlag>(flags, initPattern);
      std::strncpy(it->second->name, it->first.c_str(), 32);
    }

    return {it->second.get(), inserted};
  }

  rx::Ref<EventFlag> findEventFlag(std::string_view name) {
    std::lock_guard lock(m_evf_mtx);

    if (auto it = m_event_flags.find(name); it != m_event_flags.end()) {
      return it->second;
    }

    return {};
  }

  std::pair<Semaphore *, bool> createSemaphore(utils::kstring name,
                                               std::uint32_t attrs,
                                               std::int32_t initCount,
                                               std::int32_t maxCount) {
    std::lock_guard lock(m_sem_mtx);
    auto [it, inserted] = m_semaphores.try_emplace(std::move(name), nullptr);
    if (inserted) {
      it->second = knew<Semaphore>(attrs, initCount, maxCount);
    }

    return {it->second.get(), inserted};
  }

  rx::Ref<Semaphore> findSemaphore(std::string_view name) {
    std::lock_guard lock(m_sem_mtx);
    if (auto it = m_semaphores.find(name); it != m_semaphores.end()) {
      return it->second;
    }

    return {};
  }

  std::pair<rx::Ref<IpmiServer>, ErrorCode>
  createIpmiServer(utils::kstring name) {
    std::lock_guard lock(m_sem_mtx);
    auto [it, inserted] = mIpmiServers.try_emplace(std::move(name), nullptr);

    if (!inserted) {
      return {it->second, ErrorCode::EXIST};
    }

    it->second = knew<IpmiServer>(it->first);

    if (it->second == nullptr) {
      mIpmiServers.erase(it);
      return {nullptr, ErrorCode::NOMEM};
    }

    return {it->second, {}};
  }

  rx::Ref<IpmiServer> findIpmiServer(std::string_view name) {
    std::lock_guard lock(m_sem_mtx);
    if (auto it = mIpmiServers.find(name); it != mIpmiServers.end()) {
      return it->second;
    }

    return {};
  }

  std::tuple<utils::kmap<utils::kstring, char[128]> &,
             std::unique_lock<rx::shared_mutex>>
  getKernelEnv() {
    std::unique_lock lock(m_kenv_mtx);
    return {m_kenv, std::move(lock)};
  }

  void setKernelEnv(std::string_view key, std::string_view value) {
    auto &kenvValue = m_kenv[utils::kstring(key)];
    auto len = std::min(sizeof(kenvValue) - 1, value.size());
    std::memcpy(kenvValue, value.data(), len);
    kenvValue[len] = '0';
  }

  enum {
    c_golden_ratio_prime = 2654404609u,
    c_umtx_chains = 512,
    c_umtx_shifts = 23,
  };

  // Use getUmtxChain0 or getUmtxChain1
  std::tuple<UmtxChain &, UmtxKey, std::unique_lock<rx::shared_mutex>>
  getUmtxChainIndexed(int i, Thread *t, uint32_t flags, void *ptr);

  // Internal Umtx: Wait/Cv/Sem
  auto getUmtxChain0(Thread *t, uint32_t flags, void *ptr) {
    return getUmtxChainIndexed(0, t, flags, ptr);
  }

  // Internal Umtx: Mutex/Umtx/Rwlock
  auto getUmtxChain1(Thread *t, uint32_t flags, void *ptr) {
    return getUmtxChainIndexed(1, t, flags, ptr);
  }

  rx::Ref<EventEmitter> deviceEventEmitter;
  rx::Ref<rx::RcBase> shmDevice;
  rx::Ref<rx::RcBase> dmemDevice;
  rx::Ref<rx::RcBase> blockpoolDevice;
  rx::Ref<rx::RcBase> gpuDevice;
  rx::Ref<rx::RcBase> dceDevice;
  rx::shared_mutex gpuDeviceMtx;
  uint sdkVersion{};
  uint fwSdkVersion{};
  uint safeMode{};
  rx::RcIdMap<rx::RcBase, sint, 4097, 1> ipmiMap;
  rx::RcIdMap<RcAppInfo> appInfos;
  rx::RcIdMap<Budget, sint, 4097, 1> budgets;
  rx::Ref<Budget> processTypeBudgets[4];

  rx::shared_mutex regMgrMtx;
  kmap<std::uint32_t, std::uint32_t> regMgrInt;
  std::vector<std::tuple<std::uint8_t *, size_t>> dialogs{};

  FwType fwType = FwType::Unknown;
  bool isDevKit = false;

  rx::Ref<Budget> createProcessTypeBudget(Budget::ProcessType processType,
                                          std::string_view name,
                                          std::span<const BudgetInfo> items) {
    auto budget = orbis::knew<orbis::Budget>(name, processType, items);
    processTypeBudgets[static_cast<int>(processType)] =
        orbis::knew<orbis::Budget>(name, processType, items);
    return budget;
  }

  rx::Ref<Budget> getProcessTypeBudget(Budget::ProcessType processType) {
    return processTypeBudgets[static_cast<int>(processType)];
  }

private:
  rx::shared_mutex m_heap_mtx;
  rx::shared_mutex m_heap_map_mtx;
  void *m_heap_next = this + 1;

  utils::kmultimap<std::size_t, void *> m_free_heap;
  utils::kmultimap<std::size_t, void *> m_used_node;

  UmtxChain m_umtx_chains[2][c_umtx_chains]{};

  std::atomic<long> m_tsc_freq{0};

  rx::shared_mutex m_thread_id_mtx;
  rx::OwningIdMap<char, long, 256, 0> m_thread_id_map;
  mutable rx::shared_mutex m_proc_mtx;
  rx::LinkedNode<Process> *m_processes = nullptr;

  rx::shared_mutex m_evf_mtx;
  utils::kmap<utils::kstring, rx::Ref<EventFlag>> m_event_flags;

  rx::shared_mutex m_sem_mtx;
  utils::kmap<utils::kstring, rx::Ref<Semaphore>> m_semaphores;

  rx::shared_mutex mIpmiServerMtx;
  utils::kmap<utils::kstring, rx::Ref<IpmiServer>> mIpmiServers;

  rx::shared_mutex m_kenv_mtx;
  utils::kmap<utils::kstring, char[128]> m_kenv; // max size: 127 + '\0'
};

extern KernelContext &g_context;
} // namespace orbis
