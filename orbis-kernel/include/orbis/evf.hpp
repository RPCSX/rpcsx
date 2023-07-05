#pragma once
#include "KernelAllocator.hpp"
#include "thread/Thread.hpp"
#include "utils/SharedMutex.hpp"
#include <atomic>
#include <condition_variable>

namespace orbis {
enum {
  kEvfAttrThFifo = 0x01,
  kEvfAttrThPrio = 0x02,
  kEvfAttrSingle = 0x10,
  kEvfAttrMulti = 0x20,
};

enum {
  kEvfWaitModeAnd = 0x01,
  kEvfWaitModeOr = 0x02,
  kEvfWaitModeClearAll = 0x10,
  kEvfWaitModeClearPat = 0x20,
};

struct EventFlag {
  char name[32];

  bool isDeleted = false;
  std::uint8_t attrs;
  std::atomic<unsigned> references{0};
  std::atomic<std::uint64_t> value;

  struct WaitingThread {
    Thread *thread;
    shared_mutex *mtx;
    std::uint64_t *patternSet;
    bool *isCanceled;
    std::uint64_t bitPattern;
    std::uint8_t waitMode;

    bool test(std::uint64_t value) const {
      if (waitMode & kEvfWaitModeAnd) {
        return (value & bitPattern) == bitPattern;
      }

      return (value & bitPattern) != 0;
    }

    std::uint64_t applyClear(std::uint64_t value) {
      if (waitMode & kEvfWaitModeClearAll) {
        return 0;
      }

      if (waitMode & kEvfWaitModeClearPat) {
        return value & ~bitPattern;
      }

      return value;
    }
  };

  WaitingThread waitingThreads[32]; // TODO: create vector?
  std::atomic<std::uint32_t> waitingThreadsCount = 0;

  shared_mutex queueMtx;

  enum class NotifyType {
    Set,
    Cancel,
    Destroy
  };

  explicit EventFlag(std::int32_t attrs) : attrs(attrs) {}

  ErrorCode wait(Thread *thread, std::uint8_t waitMode,
                 std::uint64_t bitPattern, std::uint64_t *patternSet,
                 std::uint32_t *timeout);
  ErrorCode tryWait(Thread *thread, std::uint8_t waitMode,
                    std::uint64_t bitPattern, std::uint64_t *patternSet);
  std::size_t notify(NotifyType type, std::uint64_t bits);

  std::size_t destroy() {
    return notify(NotifyType::Destroy, {});
  }

  std::size_t cancel(std::uint64_t value) {
    return notify(NotifyType::Cancel, value);
  }

  std::size_t set(std::uint64_t bits) {
    return notify(NotifyType::Set, bits);
  }

  void clear(std::uint64_t bits) {
    writer_lock lock(queueMtx);
    value.fetch_and(bits, std::memory_order::relaxed);
  }

  void incRef() { references.fetch_add(1, std::memory_order::relaxed); }

  void decRef() {
    if (references.fetch_sub(1, std::memory_order::relaxed) == 1) {
      kdelete(this);
    }
  }
};
} // namespace orbis
