#pragma once

#include "KernelAllocator.hpp"
#include "thread/Thread.hpp"
#include "utils/SharedCV.hpp"
#include "utils/SharedMutex.hpp"
#include <atomic>
#include <condition_variable>

namespace orbis {
enum {
  kSemaAttrThFifo = 1,
  kSemaAttrThPrio = 2,
  kSemaAttrShared = 256,
};

struct Semaphore final {
  char name[32];

  bool isDeleted = false;
  std::uint8_t attrs;
  std::atomic<unsigned> references{0};
  std::atomic<sint> value;
  const sint maxValue;
  utils::shared_mutex mtx;
  utils::shared_cv cond;

  Semaphore(uint attrs, sint value, sint max)
      : attrs(attrs), value(value), maxValue(max) {}

  void destroy() {
    std::lock_guard lock(mtx);
    isDeleted = true;
    cond.notify_all(mtx);
  }

  void incRef() { references.fetch_add(1, std::memory_order::relaxed); }

  void decRef() {
    if (references.fetch_sub(1, std::memory_order::relaxed) == 1) {
      kdelete(this);
    }
  }
};
} // namespace orbis