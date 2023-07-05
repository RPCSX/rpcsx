#pragma once
#include "utils/LinkedNode.hpp"
#include "utils/SharedMutex.hpp"
#include "evf.hpp"

#include "orbis/thread/types.hpp"
#include "KernelAllocator.hpp"
#include <algorithm>
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

  static void *kalloc(std::size_t size,
                      std::size_t align = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  static void kfree(void *ptr, std::size_t size);

  std::pair<EventFlag *, bool> createEventFlag(utils::kstring name, std::int32_t flags) {
    auto [it, inserted] = m_event_flags.try_emplace(std::move(name), knew<EventFlag>(flags));
    return { it->second.get(), inserted };
  }

  Ref<EventFlag> findEventFlag(std::string_view name) {
    if (auto it = m_event_flags.find(name); it != m_event_flags.end()) {
      return it->second;
    }

    return{};
  }

private:
  mutable shared_mutex m_proc_mtx;
  utils::LinkedNode<Process> *m_processes = nullptr;
  utils::kmap<utils::kstring, Ref<EventFlag>> m_event_flags;

  struct node {
    std::size_t size;
    node *next;
  };

  mutable shared_mutex m_heap_mtx;
  void *m_heap_next = this + 1;
  node *m_free_list = nullptr;
};

extern KernelContext &g_context;
} // namespace orbis
