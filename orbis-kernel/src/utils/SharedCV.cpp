#include "orbis/utils/SharedCV.hpp"
#include <chrono>

#ifdef ORBIS_HAS_FUTEX
#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>
#endif

namespace orbis::utils {
std::errc shared_cv::impl_wait(shared_mutex &mutex, unsigned _val,
                               std::uint64_t usec_timeout) noexcept {
  // Not supposed to fail
  if (!_val) {
    std::abort();
  }

  std::errc result = {};

  bool useTimeout = usec_timeout != static_cast<std::uint64_t>(-1);

  while (true) {
    result =
        m_value.wait(_val, useTimeout ? std::chrono::microseconds(usec_timeout)
                                      : std::chrono::microseconds::max());
    bool spurious = result == std::errc::resource_unavailable_try_again ||
                    result == std::errc::interrupted;

    // Cleanup
    const auto old = m_value.fetch_op([&](unsigned &value) {
      // Remove waiter if no signals
      if ((value & ~c_waiter_mask) == 0) {

        if (!spurious) {
          value -= 1;
        }
      }

      // Try to remove signal
      if (value & c_signal_mask) {
        value -= c_signal_one;
      }

#ifdef ORBIS_HAS_FUTEX
      if (value & c_locked_mask) {
        value -= c_locked_mask;
      }
#endif
    });

#ifdef ORBIS_HAS_FUTEX
    // Lock is already acquired
    if (old & c_locked_mask) {
      return {};
    }

    // Wait directly (waiter has been added)
    if (old & c_signal_mask) {
      return mutex.impl_wait();
    }
#else
    if (old & c_signal_mask) {
      result = {};
      break;
    }
#endif

    // Possibly spurious wakeup
    if (!spurious) {
      break;
    }

    _val = old;
  }

  mutex.lock();
  return result;
}

void shared_cv::impl_wake(shared_mutex &mutex, int _count) noexcept {
#ifdef ORBIS_HAS_FUTEX
  while (true) {
    unsigned _old = m_value.load();
    const bool is_one = _count == 1;

    // Enqueue _count waiters
    _count = std::min<int>(_count, _old & c_waiter_mask);
    if (_count <= 0)
      return;

    // Try to lock the mutex
    const bool locked = mutex.lock_forced(_count);

    const int max_sig = m_value.op([&](unsigned &value) {
      // Verify the number of waiters
      int max_sig = std::min<int>(_count, value & c_waiter_mask);

      // Add lock signal (mutex was immediately locked)
      if (locked && max_sig)
        value |= c_locked_mask;
      else if (locked)
        std::abort();

      // Add normal signals
      value += c_signal_one * max_sig;

      // Remove waiters
      value -= max_sig;
      _old = value;
      return max_sig;
    });

    if (max_sig < _count) {
      // Fixup mutex
      mutex.lock_forced(max_sig - _count);
      _count = max_sig;
    }

    if (_count) {
      // Wake up one thread + requeue remaining waiters
      unsigned awake_count = locked ? 1 : 0;
      if (auto r = syscall(SYS_futex, &m_value, FUTEX_REQUEUE, awake_count,
                           _count - awake_count, &mutex, 0);
          r < _count) {
        // Keep awaking waiters
        _count = is_one ? 1 : INT_MAX;
        continue;
      }
    }

    break;
  }
#else
  unsigned _old = m_value.load();
  _count = std::min<int>(_count, _old & c_waiter_mask);
  if (_count <= 0)
    return;

  mutex.lock_forced(1);

  const int wakeupWaiters = m_value.op([&](unsigned &value) {
    int max_sig = std::min<int>(_count, value & c_waiter_mask);

    // Add normal signals
    value += c_signal_one * max_sig;

    // Remove waiters
    value -= max_sig;
    _old = value;
    return max_sig;
  });

  if (wakeupWaiters > 0) {
    m_value.notify_n(wakeupWaiters);
  }

  mutex.unlock();
#endif
}
} // namespace orbis::utils
