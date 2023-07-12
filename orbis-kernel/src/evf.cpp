#include "evf.hpp"
#include "error/ErrorCode.hpp"
#include "utils/Logs.hpp"
#include <atomic>

orbis::ErrorCode orbis::EventFlag::wait(Thread *thread, std::uint8_t waitMode,
                                        std::uint64_t bitPattern,
                                        std::uint64_t *patternSet,
                                        std::uint32_t *timeout) {
  if (timeout != nullptr) {
    ORBIS_LOG_FATAL("evf: timeout is not implemented");
    std::abort();
  }

  shared_mutex mtx;
  writer_lock lock(mtx);

  bool isCanceled = false;

  {
    writer_lock lock(queueMtx);

    if (isDeleted) {
      return ErrorCode::ACCES;
    }

    auto waitingThread = WaitingThread{.thread = thread,
                                       .mtx = &mtx,
                                       .patternSet = patternSet,
                                       .isCanceled = &isCanceled,
                                       .bitPattern = bitPattern,
                                       .waitMode = waitMode};

    if (auto patValue = value.load(std::memory_order::relaxed);
        waitingThread.test(patValue)) {
      auto resultValue = waitingThread.applyClear(patValue);
      value.store(resultValue, std::memory_order::relaxed);
      if (patternSet != nullptr) {
        *patternSet = resultValue;
      }
      return {};
    }

    std::size_t position;

    if (attrs & kEvfAttrSingle) {
      if (waitingThreadsCount.fetch_add(1, std::memory_order::relaxed) != 0) {
        waitingThreadsCount.store(1, std::memory_order::relaxed);
        return ErrorCode::PERM;
      }

      position = 0;
    } else {
      if (attrs & kEvfAttrThFifo) {
        position = waitingThreadsCount.fetch_add(1, std::memory_order::relaxed);
      } else {
        // FIXME: sort waitingThreads by priority
        position = waitingThreadsCount.fetch_add(1, std::memory_order::relaxed);
      }
    }

    if (position >= std::size(waitingThreads)) {
      std::abort();
    }

    waitingThreads[position] = waitingThread;
  }

  // TODO: update thread state

  mtx.lock(); // HACK: lock self to wait unlock from another thread :)

  if (isCanceled) {
    return ErrorCode::CANCELED;
  }

  if (isDeleted) {
    return ErrorCode::ACCES;
  }

  return {};
}

orbis::ErrorCode orbis::EventFlag::tryWait(Thread *, std::uint8_t waitMode,
                                           std::uint64_t bitPattern,
                                           std::uint64_t *patternSet) {
  writer_lock lock(queueMtx);

  if (isDeleted) {
    return ErrorCode::ACCES;
  }

  auto waitingThread =
      WaitingThread{.bitPattern = bitPattern, .waitMode = waitMode};

  if (auto patValue = value.load(std::memory_order::relaxed);
      waitingThread.test(patValue)) {
    auto resultValue = waitingThread.applyClear(patValue);
    value.store(resultValue, std::memory_order::relaxed);
    if (patternSet != nullptr) {
      *patternSet = resultValue;
    }

    return {};
  }

  return ErrorCode::BUSY;
}

std::size_t orbis::EventFlag::notify(NotifyType type, std::uint64_t bits) {
  writer_lock lock(queueMtx);
  auto count = waitingThreadsCount.load(std::memory_order::relaxed);
  auto patValue = value.load(std::memory_order::relaxed);

  if (type == NotifyType::Destroy) {
    isDeleted = true;
  } else if (type == NotifyType::Set) {
    patValue |= bits;
  }

  auto testThread = [&](WaitingThread *thread) {
    if (type == NotifyType::Set && !thread->test(patValue)) {
      return false;
    }

    auto resultValue = thread->applyClear(patValue);
    patValue = resultValue;
    if (thread->patternSet != nullptr) {
      *thread->patternSet = resultValue;
    }
    if (type == NotifyType::Cancel) {
      *thread->isCanceled = true;
    }

    // TODO: update thread state
    if (thread->mtx->is_free())
      std::abort();
    thread->mtx->unlock(); // release wait on waiter thread

    waitingThreadsCount.fetch_sub(1, std::memory_order::relaxed);
    std::memmove(thread, thread + 1,
                 (waitingThreads + count - (thread + 1)) *
                     sizeof(*waitingThreads));
    --count;
    return true;
  };

  std::size_t result = 0;
  if (attrs & kEvfAttrThFifo) {
    for (std::size_t i = count; i > 0;) {
      if (!testThread(waitingThreads + i - 1)) {
        --i;
        continue;
      }

      ++result;
    }
  } else {
    for (std::size_t i = 0; i < count;) {
      if (!testThread(waitingThreads + i)) {
        ++i;
        continue;
      }

      ++result;
    }
  }

  if (type == NotifyType::Cancel) {
    value.store(bits, std::memory_order::relaxed);
  } else {
    value.store(patValue, std::memory_order::relaxed);
  }
  return result;
}
