#pragma once
#include "evf.hpp"
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
  void *addr;
  pid_t pid;

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
  void notify_one(const UmtxKey &key);
  void notify_all(const UmtxKey &key);
};

class alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) KernelContext final {
public:
  KernelContext();
  ~KernelContext();

  Process *createProcess(pid_t pid);
  void deleteProcess(Process *proc);
  Process *findProcessById(pid_t pid) const;

  void *kalloc(std::size_t size,
               std::size_t align = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  void kfree(void *ptr, std::size_t size);

  std::pair<EventFlag *, bool> createEventFlag(utils::kstring name,
                                               std::int32_t flags) {
    std::lock_guard lock(m_evf_mtx);

    auto [it, inserted] = m_event_flags.try_emplace(std::move(name), nullptr);
    if (inserted) {
      it->second = knew<EventFlag>(flags);
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

  enum {
    c_golden_ratio_prime = 2654404609u,
    c_umtx_chains = 512,
    c_umtx_shifts = 23,
  };

  // Use getUmtxChain0 or getUmtxChain1
  std::tuple<UmtxChain &, UmtxKey, std::unique_lock<shared_mutex>>
  getUmtxChainIndexed(int i, pid_t pid, void *ptr) {
    auto n = reinterpret_cast<std::uintptr_t>(ptr) + pid;
    n = ((n * c_golden_ratio_prime) >> c_umtx_shifts) % c_umtx_chains;
    std::unique_lock lock(m_umtx_chains[i][n].mtx);
    return {m_umtx_chains[i][n], UmtxKey{ptr, pid}, std::move(lock)};
  }

  // Internal Umtx: Wait/Cv/Sem
  auto getUmtxChain0(pid_t pid, void *ptr) {
    return getUmtxChainIndexed(0, pid, ptr);
  }

  // Internal Umtx: Mutex/Umtx/Rwlock
  auto getUmtxChain1(pid_t pid, void *ptr) {
    return getUmtxChainIndexed(1, pid, ptr);
  }

private:
  shared_mutex m_evf_mtx;
  mutable pthread_mutex_t m_heap_mtx;
  void *m_heap_next = this + 1;
  bool m_heap_is_freeing = false;
  utils::kmultimap<std::size_t, void *> m_free_heap;
  utils::kmultimap<std::size_t, void *> m_used_node;

  UmtxChain m_umtx_chains[2][c_umtx_chains]{};

  mutable shared_mutex m_proc_mtx;
  utils::LinkedNode<Process> *m_processes = nullptr;
  utils::kmap<utils::kstring, Ref<EventFlag>> m_event_flags;
};

extern KernelContext &g_context;
} // namespace orbis
