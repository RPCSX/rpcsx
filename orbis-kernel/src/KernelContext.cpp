#include "orbis/KernelContext.hpp"
#include "orbis/thread/Process.hpp"
#include <sys/mman.h>
#include <sys/unistd.h>

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

KernelContext::KernelContext() {
  // Initialize recursive heap mutex
  pthread_mutexattr_t mtx_attr;
  pthread_mutexattr_init(&mtx_attr);
  pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&m_heap_mtx, &mtx_attr);
  pthread_mutexattr_destroy(&mtx_attr);

  std::printf("orbis::KernelContext initialized, addr=%p", this);
}
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
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  if (!size)
    std::abort();

  pthread_mutex_lock(&m_heap_mtx);
  if (!m_heap_is_freeing) {
    // Try to reuse previously freed block
    for (auto [it, end] = m_free_heap.equal_range(size); it != end; it++) {
      auto result = it->second;
      if (!(reinterpret_cast<std::uintptr_t>(result) & (align - 1))) {
        auto node = m_free_heap.extract(it);
        node.key() = 0;
        node.mapped() = nullptr;
        m_used_node.insert(m_used_node.begin(), std::move(node));
        pthread_mutex_unlock(&m_heap_mtx);
        return result;
      }
    }
  }

  align = std::max<std::size_t>(align, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  auto heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
  heap = (heap + (align - 1)) & ~(align - 1);
  auto result = reinterpret_cast<void *>(heap);
  m_heap_next = reinterpret_cast<void *>(heap + size);
  pthread_mutex_unlock(&m_heap_mtx);
  return result;
}

void KernelContext::kfree(void *ptr, std::size_t size) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  if (!size)
    std::abort();

  pthread_mutex_lock(&m_heap_mtx);
  if (m_heap_is_freeing)
    std::abort();
  m_heap_is_freeing = true;
  if (!m_used_node.empty()) {
    auto node = m_used_node.extract(m_used_node.begin());
    node.key() = size;
    node.mapped() = ptr;
    m_free_heap.insert(std::move(node));
  } else {
    m_free_heap.emplace(size, ptr);
  }
  m_heap_is_freeing = false;
  pthread_mutex_unlock(&m_heap_mtx);
}

inline namespace utils {
void kfree(void *ptr, std::size_t size) { return g_context.kfree(ptr, size); }
void *kalloc(std::size_t size, std::size_t align) {
  return g_context.kalloc(size, align);
}
} // namespace utils
} // namespace orbis
