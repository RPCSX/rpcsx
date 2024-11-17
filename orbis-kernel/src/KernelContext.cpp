#include "orbis/KernelContext.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/ProcessOps.hpp"
#include "orbis/utils/Logs.hpp"
#include "utils/SharedAtomic.hpp"
#include <bit>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

static const std::uint64_t g_allocProtWord = 0xDEADBEAFBADCAFE1;
static constexpr std::uintptr_t kHeapBaseAddress = 0x00000600'0000'0000;
static constexpr auto kHeapSize = 0x1'0000'0000;
static constexpr int kDebugHeap = 0;

namespace orbis {
thread_local Thread *g_currentThread;

KernelContext &g_context = *[]() -> KernelContext * {
  // Allocate global shared kernel memory
  // TODO: randomize for hardening and reduce size
  auto ptr = mmap(std::bit_cast<void *>(kHeapBaseAddress), kHeapSize,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (ptr == MAP_FAILED) {
    perror("mmap failed");
    FILE *maps = fopen("/proc/self/maps", "r");
    char *line = nullptr;
    std::size_t size = 0;
    while (getline(&line, &size, maps) > 0) {
      std::printf("%s", line);
    }

    free(line);
    fclose(maps);
    std::abort();
  }

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
      if (!(std::bit_cast<std::uintptr_t>(result) & (align - 1))) {
        auto node = m_free_heap.extract(it);
        node.key() = 0;
        node.mapped() = nullptr;
        m_used_node.insert(m_used_node.begin(), std::move(node));

        // std::fprintf(stderr, "kalloc: reuse %p-%p, size = %lx\n", result,
        //              (char *)result + size, size);

        if (kDebugHeap > 0) {
          std::memcpy(std::bit_cast<std::byte *>(result) + size,
                      &g_allocProtWord, sizeof(g_allocProtWord));
        }
        return result;
      }
    }
  }

  std::lock_guard lock(m_heap_mtx);
  align = std::max<std::size_t>(align, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  auto heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
  heap = (heap + (align - 1)) & ~(align - 1);

  if (kDebugHeap > 1) {
    if (auto diff = (heap + size + sizeof(g_allocProtWord)) % 4096; diff != 0) {
      heap += 4096 - diff;
      heap &= ~(align - 1);
    }
  }

  if (heap + size > kHeapBaseAddress + kHeapSize) {
    std::fprintf(stderr, "out of kernel memory");
    std::abort();
  }
  // Check overflow
  if (heap + size < heap) {
    std::fprintf(stderr, "too big allocation");
    std::abort();
  }

  // std::fprintf(stderr, "kalloc: allocate %lx-%lx, size = %lx, align=%lx\n",
  //              heap, heap + size, size, align);

  auto result = reinterpret_cast<void *>(heap);
  if (kDebugHeap > 0) {
    std::memcpy(std::bit_cast<std::byte *>(result) + size, &g_allocProtWord,
                sizeof(g_allocProtWord));
  }

  if (kDebugHeap > 0) {
    m_heap_next =
        reinterpret_cast<void *>(heap + size + sizeof(g_allocProtWord));
  } else {
    m_heap_next = reinterpret_cast<void *>(heap + size);
  }

  if (kDebugHeap > 1) {
    heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
    align = std::min<std::size_t>(align, 4096);
    heap = (heap + (align - 1)) & ~(align - 1);
    size = 4096;
    // std::fprintf(stderr, "kalloc: protect %lx-%lx, size = %lx, align=%lx\n",
    //              heap, heap + size, size, align);

    auto result = ::mmap(reinterpret_cast<void *>(heap), size, PROT_NONE,
                         MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (result == MAP_FAILED) {
      std::fprintf(stderr, "failed to protect memory");
      std::abort();
    }
    m_heap_next = reinterpret_cast<void *>(heap + size);
  }

  return result;
}

void KernelContext::kfree(void *ptr, std::size_t size) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  if (!size)
    std::abort();

  if (std::bit_cast<std::uintptr_t>(ptr) < kHeapBaseAddress ||
      std::bit_cast<std::uintptr_t>(ptr) + size >
          kHeapBaseAddress + kHeapSize) {
    std::fprintf(stderr, "kfree: invalid address");
    std::abort();
  }

  if (kDebugHeap > 0) {
    if (std::memcmp(std::bit_cast<std::byte *>(ptr) + size, &g_allocProtWord,
                    sizeof(g_allocProtWord)) != 0) {
      std::fprintf(stderr, "kernel heap corruption\n");
      std::abort();
    }

    std::memset(ptr, 0xcc, size + sizeof(g_allocProtWord));
  }

  // std::fprintf(stderr, "kfree: release %p-%p, size = %lx\n", ptr,
  //              (char *)ptr + size, size);

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
  if (pthread_sigqueue(getNativeHandle(), SIGUSR1, {.sival_int = signo})) {
    perror("pthread_sigqueue");
  }
}

void Thread::notifyUnblockedSignal(int signo) {
  for (std::size_t i = 0; i < blockedSignals.size();) {
    if (blockedSignals[i].signo != signo) {
      ++i;
      continue;
    }

    queuedSignals.push_back(blockedSignals[i]);
    blockedSignals.erase(blockedSignals.begin() + i);
  }
}

void Thread::where() { tproc->ops->where(this); }

void Thread::unblock() { tproc->ops->unblock(this); }
void Thread::block() { tproc->ops->block(this); }

scoped_unblock::scoped_unblock() {
  if (g_currentThread && g_currentThread->context) {
    g_scopedUnblock = [](bool unblock) {
      if (unblock) {
        g_currentThread->unblock();
      } else {
        g_currentThread->block();
      }
    };
  }
}

scoped_unblock::~scoped_unblock() { g_scopedUnblock = nullptr; }
} // namespace orbis
