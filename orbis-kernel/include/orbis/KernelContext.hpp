#pragma once
#include "evf.hpp"
#include "utils/LinkedNode.hpp"
#include "utils/SharedMutex.hpp"

#include "KernelAllocator.hpp"
#include "orbis/thread/types.hpp"
#include <algorithm>
#include <pthread.h>
#include <utility>

namespace orbis {
struct Process;

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

private:
  shared_mutex m_evf_mtx;
  mutable pthread_mutex_t m_heap_mtx;
  void *m_heap_next = this + 1;
  bool m_heap_is_freeing = false;
  utils::kmultimap<std::size_t, void *> m_free_heap;
  utils::kmultimap<std::size_t, void *> m_used_node;

  mutable shared_mutex m_proc_mtx;
  utils::LinkedNode<Process> *m_processes = nullptr;
  utils::kmap<utils::kstring, Ref<EventFlag>> m_event_flags;
};

extern KernelContext &g_context;
} // namespace orbis
