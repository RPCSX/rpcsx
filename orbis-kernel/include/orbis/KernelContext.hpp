#pragma once
#include "evf.hpp"
#include "osem.hpp"
#include "utils/LinkedNode.hpp"
#include "utils/SharedCV.hpp"
#include "utils/SharedMutex.hpp"

#include "KernelAllocator.hpp"
#include "orbis/thread/types.hpp"
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <pthread.h>
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
  utils::shared_cv cv;

  UmtxCond(Thread *thr) : thr(thr) {}
};

struct UmtxChain {
  utils::shared_mutex mtx;
  utils::kmultimap<UmtxKey, UmtxCond> sleep_queue;
  utils::kmultimap<UmtxKey, UmtxCond> spare_queue;

  std::pair<const UmtxKey, UmtxCond> *enqueue(UmtxKey &key, Thread *thr);
  void erase(std::pair<const UmtxKey, UmtxCond> *obj);
  uint notify_one(const UmtxKey &key);
  uint notify_all(const UmtxKey &key);
};

class alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) KernelContext final {
public:
  KernelContext();
  ~KernelContext();

  Process *createProcess(pid_t pid);
  void deleteProcess(Process *proc);
  Process *findProcessById(pid_t pid) const;

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

  Ref<EventFlag> findEventFlag(std::string_view name) {
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

  Ref<Semaphore> findSemaphore(std::string_view name) {
    std::lock_guard lock(m_sem_mtx);
    if (auto it = m_semaphores.find(name); it != m_semaphores.end()) {
      return it->second;
    }

    return {};
  }

  enum {
    c_golden_ratio_prime = 2654404609u,
    c_umtx_chains = 512,
    c_umtx_shifts = 23,
  };

  // Use getUmtxChain0 or getUmtxChain1
  std::tuple<UmtxChain &, UmtxKey, std::unique_lock<shared_mutex>>
  getUmtxChainIndexed(int i, Thread *t, uint32_t flags, void *ptr);

  // Internal Umtx: Wait/Cv/Sem
  auto getUmtxChain0(Thread *t, uint32_t flags, void *ptr) {
    return getUmtxChainIndexed(0, t, flags, ptr);
  }

  // Internal Umtx: Mutex/Umtx/Rwlock
  auto getUmtxChain1(Thread *t, uint32_t flags, void *ptr) {
    return getUmtxChainIndexed(1, t, flags, ptr);
  }

private:
  mutable pthread_mutex_t m_heap_mtx;
  void *m_heap_next = this + 1;
  bool m_heap_is_freeing = false;
  utils::kmultimap<std::size_t, void *> m_free_heap;
  utils::kmultimap<std::size_t, void *> m_used_node;

  UmtxChain m_umtx_chains[2][c_umtx_chains]{};

  std::atomic<long> m_tsc_freq{0};

  mutable shared_mutex m_proc_mtx;
  utils::LinkedNode<Process> *m_processes = nullptr;

  shared_mutex m_evf_mtx;
  utils::kmap<utils::kstring, Ref<EventFlag>> m_event_flags;

  shared_mutex m_sem_mtx;
  utils::kmap<utils::kstring, Ref<Semaphore>> m_semaphores;
};

extern KernelContext &g_context;
} // namespace orbis
