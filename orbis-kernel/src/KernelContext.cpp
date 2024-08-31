#include "orbis/KernelContext.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/ProcessOps.hpp"
#include "orbis/utils/Logs.hpp"
#include <chrono>
#include <csignal>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

static const std::uint64_t g_allocProtWord = 0xDEADBEAFBADCAFE1;

namespace orbis {
thread_local Thread *g_currentThread;

KernelContext &g_context = *[]() -> KernelContext * {
  // Allocate global shared kernel memory
  // TODO: randomize for hardening and reduce size
  auto ptr = mmap(reinterpret_cast<void *>(0x200'0000'0000), 0x2'0000'0000,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (ptr == MAP_FAILED)
    std::abort();

  return new (ptr) KernelContext;
}();

KernelContext::KernelContext() {
  // std::printf("orbis::KernelContext initialized, addr=%p\n", this);
  // std::printf("TSC frequency: %lu\n", getTscFreq());
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
  for (std::size_t i = 0; i < 20; ++i) {
    {
      std::lock_guard lock(m_proc_mtx);
      for (auto proc = m_processes; proc != nullptr; proc = proc->next) {
        if (proc->object.pid == pid) {
          return &proc->object;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }

  return nullptr;
}

Process *KernelContext::findProcessByHostId(std::uint64_t pid) const {
  for (std::size_t i = 0; i < 20; ++i) {
    {
      std::lock_guard lock(m_proc_mtx);
      for (auto proc = m_processes; proc != nullptr; proc = proc->next) {
        if (proc->object.hostPid == pid) {
          return &proc->object;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }

  return nullptr;
}

long KernelContext::getTscFreq() {
  auto cal_tsc = []() -> long {
    const long timer_freq = 1'000'000'000;

    // Calibrate TSC
    constexpr int samples = 40;
    long rdtsc_data[samples];
    long timer_data[samples];
    long error_data[samples];

    struct ::timespec ts0;
    clock_gettime(CLOCK_MONOTONIC, &ts0);
    long sec_base = ts0.tv_sec;

    for (int i = 0; i < samples; i++) {
      usleep(200);
      error_data[i] = (__builtin_ia32_lfence(), __builtin_ia32_rdtsc());
      struct ::timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      rdtsc_data[i] = (__builtin_ia32_lfence(), __builtin_ia32_rdtsc());
      timer_data[i] = ts.tv_nsec + (ts.tv_sec - sec_base) * 1'000'000'000;
    }

    // Compute average TSC
    long acc = 0;
    for (int i = 0; i < samples - 1; i++) {
      acc += (rdtsc_data[i + 1] - rdtsc_data[i]) * timer_freq /
             (timer_data[i + 1] - timer_data[i]);
    }

    // Rounding
    acc /= (samples - 1);
    constexpr long grain = 1'000'000;
    return grain * (acc / grain + long{(acc % grain) > (grain / 2)});
  };

  long freq = m_tsc_freq.load();
  if (freq)
    return freq;
  m_tsc_freq.compare_exchange_strong(freq, cal_tsc());
  return m_tsc_freq.load();
}

void *KernelContext::kalloc(std::size_t size, std::size_t align) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  if (!size)
    std::abort();

  if (m_heap_map_mtx.try_lock()) {
    std::lock_guard lock(m_heap_map_mtx, std::adopt_lock);

    // Try to reuse previously freed block
    for (auto [it, end] = m_free_heap.equal_range(size); it != end; ++it) {
      auto result = it->second;
      if (!(reinterpret_cast<std::uintptr_t>(result) & (align - 1))) {
        auto node = m_free_heap.extract(it);
        node.key() = 0;
        node.mapped() = nullptr;
        m_used_node.insert(m_used_node.begin(), std::move(node));
        return result;
      }
    }
  }

  std::lock_guard lock(m_heap_mtx);
  align = std::max<std::size_t>(align, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  auto heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
  heap = (heap + (align - 1)) & ~(align - 1);
  auto result = reinterpret_cast<void *>(heap);
  std::memcpy(std::bit_cast<std::byte *>(result) + size, &g_allocProtWord,
              sizeof(g_allocProtWord));
  m_heap_next = reinterpret_cast<void *>(heap + size + sizeof(g_allocProtWord));
  // Check overflow
  if (heap + size < heap)
    std::abort();
  if (heap + size > (uintptr_t)&g_context + 0x1'0000'0000)
    std::abort();
  return result;
}

void KernelContext::kfree(void *ptr, std::size_t size) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  if (!size)
    std::abort();

  std::memset(ptr, 0xcc, size);

  if (std::memcmp(std::bit_cast<std::byte *>(ptr) + size, &g_allocProtWord,
                  sizeof(g_allocProtWord)) != 0) {
    std::fprintf(stderr, "kernel heap corruption\n");
    std::abort();
  }

  std::lock_guard lock(m_heap_map_mtx);
  if (!m_used_node.empty()) {
    auto node = m_used_node.extract(m_used_node.begin());
    node.key() = size;
    node.mapped() = ptr;
    m_free_heap.insert(std::move(node));
  } else {
    m_free_heap.emplace(size, ptr);
  }
}

std::tuple<UmtxChain &, UmtxKey, std::unique_lock<shared_mutex>>
KernelContext::getUmtxChainIndexed(int i, Thread *t, uint32_t flags,
                                   void *ptr) {
  auto pid = t->tproc->pid;
  auto p = reinterpret_cast<std::uintptr_t>(ptr);
  if (flags & 1) {
    pid = 0; // Process shared (TODO)
    ORBIS_LOG_WARNING("Using process-shared umtx", t->tid, ptr, (p % 0x4000));
    t->where();
  }
  auto n = p + pid;
  if (flags & 1)
    n %= 0x4000;
  n = ((n * c_golden_ratio_prime) >> c_umtx_shifts) % c_umtx_chains;
  std::unique_lock lock(m_umtx_chains[i][n].mtx);
  return {m_umtx_chains[i][n], UmtxKey{p, pid}, std::move(lock)};
}

inline namespace utils {
void kfree(void *ptr, std::size_t size) { return g_context.kfree(ptr, size); }
void *kalloc(std::size_t size, std::size_t align) {
  return g_context.kalloc(size, align);
}
} // namespace utils

inline namespace logs {
template <>
void log_class_string<kstring>::format(std::string &out, const void *arg) {
  out += get_object(arg);
}
} // namespace logs

void Thread::suspend() { sendSignal(-1); }

void Thread::resume() { sendSignal(-2); }

void Thread::sendSignal(int signo) {
  std::lock_guard lock(mtx);
  signalQueue.push_back(signo);
  if (::tgkill(tproc->hostPid, hostTid, SIGUSR1) < 0) {
    perror("tgkill");
  }
}

void Thread::where() { tproc->ops->where(this); }
} // namespace orbis
