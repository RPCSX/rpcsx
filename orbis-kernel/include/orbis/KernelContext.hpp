#pragma once
#include "utils/LinkedNode.hpp"
#include "utils/SharedMutex.hpp"

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

private:
  mutable shared_mutex m_proc_mtx;
  utils::LinkedNode<Process> *m_processes = nullptr;

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
