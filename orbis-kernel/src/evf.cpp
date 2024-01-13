#include "evf.hpp"
#include "error/ErrorCode.hpp"
#include "utils/Logs.hpp"
#include "utils/SharedCV.hpp"
#include <atomic>

orbis::ErrorCode orbis::EventFlag::wait(Thread *thread, std::uint8_t waitMode,
                                        std::uint64_t bitPattern,
                                        std::uint32_t *timeout) {
  using namespace std::chrono;

  steady_clock::time_point start{};
  uint64_t elapsed = 0;
  uint64_t fullTimeout = -1;
  if (timeout) {
    start = steady_clock::now();
    fullTimeout = *timeout;
  }

  auto update_timeout = [&] {
    if (!timeout)
      return;
    auto now = steady_clock::now();
    elapsed = duration_cast<microseconds>(now - start).count();
    if (fullTimeout > elapsed) {
      *timeout = fullTimeout - elapsed;
      return;
    }
    *timeout = 0;
  };

  thread->evfResultPattern = 0;
  thread->evfIsCancelled = -1;

  std::unique_lock lock(queueMtx);
  int result = 0;
  while (true) {
    if (isDeleted) {
      if (thread->evfIsCancelled == UINT64_MAX)
        thread->evfResultPattern = value.load();
      return ErrorCode::ACCES;
    }
    if (thread->evfIsCancelled == 1) {
      return ErrorCode::CANCELED;
    }
    if (thread->evfIsCancelled == 0) {
      break;
    }

    thread->evfResultPattern = 0;
    thread->evfIsCancelled = -1;

    auto waitingThread = WaitingThread{
        .thread = thread, .bitPattern = bitPattern, .waitMode = waitMode};

    if (auto patValue = value.load(std::memory_order::relaxed);
        waitingThread.test(patValue)) {
      auto resultValue = waitingThread.applyClear(patValue);
      value.store(resultValue, std::memory_order::relaxed);
      thread->evfResultPattern = patValue;
      // Success
      break;
    } else if (timeout && *timeout == 0) {
      thread->evfResultPattern = patValue;
      return ErrorCode::TIMEDOUT;
    }

    if (attrs & kEvfAttrSingle) {
      if (!waitingThreads.empty())
        return ErrorCode::PERM;
    } else {
      if (attrs & kEvfAttrThFifo) {
      } else {
        // FIXME: sort waitingThreads by priority
      }
    }

    waitingThreads.emplace_back(waitingThread);

    if (timeout) {
      result = thread->sync_cv.wait(queueMtx, *timeout);
      update_timeout();
    } else {
      result = thread->sync_cv.wait(queueMtx);
    }

    if (thread->evfIsCancelled == UINT64_MAX) {
      std::erase(waitingThreads, waitingThread);
    }
  }

  // TODO: update thread state
  return ErrorCode{result};
}

orbis::ErrorCode orbis::EventFlag::tryWait(Thread *thread,
                                           std::uint8_t waitMode,
                                           std::uint64_t bitPattern) {
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
    thread->evfResultPattern = patValue;
    return {};
  }

  return ErrorCode::BUSY;
}

std::size_t orbis::EventFlag::notify(NotifyType type, std::uint64_t bits) {
  writer_lock lock(queueMtx);
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
    thread->thread->evfResultPattern = patValue;
    thread->thread->evfIsCancelled = type == NotifyType::Cancel;
    patValue = resultValue;

    // TODO: update thread state
    // release wait on waiter thread
    thread->thread->sync_cv.notify_all(queueMtx);
    return true;
  };

  std::size_t result = std::erase_if(
      waitingThreads, [&](auto &thread) { return testThread(&thread); });

  if (type == NotifyType::Cancel) {
    value.store(bits, std::memory_order::relaxed);
  } else {
    value.store(patValue, std::memory_order::relaxed);
  }
  return result;
}
