#include <sys/mman.h>
#include <sys/unistd.h>

#include "orbis/KernelContext.hpp"
#include "orbis/thread/Process.hpp"

namespace orbis {
KernelContext &g_context = *[]() -> KernelContext * {
  // Allocate global shared kernel memory
  // TODO: randomize for hardening and reduce size
  auto ptr = mmap(reinterpret_cast<void *>(0x200'0000'0000), 0x1'0000'0000,
                  PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  if (!ptr)
    std::abort();

  return new (ptr) KernelContext;
  }();

  KernelContext::KernelContext() {}
  KernelContext::~KernelContext() {}

  Process *KernelContext::createProcess(pid_t pid) {
    auto newProcess = knew<utils::LinkedNode<Process>>();
    newProcess->object.context = this;
    newProcess->object.pid = pid;
    newProcess->object.state = ProcessState::NEW;

    {
      std::lock_guard lock(m_proc_mtx);
      if (m_processes != nullptr) {
        m_processes->insertPrev(*newProcess);
      }

      m_processes = newProcess;
    }

    return &newProcess->object;
  }

  void KernelContext::deleteProcess(Process *proc) {
    auto procNode = reinterpret_cast<utils::LinkedNode<Process> *>(proc);
    auto pid = proc->pid;

    {
      std::lock_guard lock(m_proc_mtx);
      auto next = procNode->erase();

      if (procNode == m_processes) {
        m_processes = next;
      }
    }

    kdelete(procNode);
  }

  Process *KernelContext::findProcessById(pid_t pid) const {
    std::lock_guard lock(m_proc_mtx);
    for (auto proc = m_processes; proc != nullptr; proc = proc->next) {
      if (proc->object.pid == pid) {
        return &proc->object;
      }
    }

    return nullptr;
  }

  void *KernelContext::kalloc(std::size_t size, std::size_t align) {
    std::lock_guard lock(g_context.m_heap_mtx);
    align = std::max(align, sizeof(node));
    auto heap = reinterpret_cast<std::uintptr_t>(g_context.m_heap_next);
    heap = (heap + (align - 1)) & ~(align - 1);
    auto result = reinterpret_cast<void *>(heap);
    g_context.m_heap_next = reinterpret_cast<void *>(heap + size);
    return result;
  }

  void KernelContext::kfree(void *ptr, std::size_t size) {
    std::lock_guard lock(g_context.m_heap_mtx);
    if (!size)
      std::abort();
    // TODO: create node and put it into
  }

  inline namespace utils {
  void kfree(void *ptr, std::size_t size) {
    return KernelContext::kfree(ptr, size);
  }
  void *kalloc(std::size_t size, std::size_t align) {
    return KernelContext::kalloc(size, align);
  }
  } // namespace utils
} // namespace orbis
