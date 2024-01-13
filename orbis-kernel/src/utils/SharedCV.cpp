#include "orbis/utils/SharedCV.hpp"
#include "orbis/utils/Logs.hpp"
#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>

namespace orbis::utils {
int shared_cv::impl_wait(shared_mutex &mutex, unsigned _val,
                          std::uint64_t usec_timeout) noexcept {
  // Not supposed to fail
  if (!_val) {
    std::abort();
  }

  // Wait with timeout
  struct timespec timeout {};
  timeout.tv_nsec = (usec_timeout % 1000'000) * 1000;
  timeout.tv_sec = (usec_timeout / 1000'000);

  int result = 0;

  while (true) {
    result = syscall(SYS_futex, &m_value, FUTEX_WAIT, _val,
                          usec_timeout + 1 ? &timeout : nullptr, 0, 0);
    if (result < 0) {
      result = errno;
    }

    // Cleanup
    const auto old = atomic_fetch_op(m_value, [&](unsigned &value) {
      // Remove waiter if no signals
      if (!(value & ~c_waiter_mask) && result != EAGAIN) {
        value -= 1;
      }

      // Try to remove signal
      if (value & c_signal_mask) {
        value -= c_signal_one;
      }

      if (value & c_locked_mask) {
        value -= c_locked_mask;
      }
    });

    // Lock is already acquired
    if (old & c_locked_mask) {
      return 0;
    }

    // Wait directly (waiter has been added)
    if (old & c_signal_mask) {
      return mutex.impl_wait();
    }

    // Possibly spurious wakeup
    if (result != EAGAIN) {
      break;
    }

    _val = old;
  }

  mutex.lock();
  return result;
}

void shared_cv::impl_wake(shared_mutex &mutex, int _count) noexcept {
  unsigned _old = m_value.load();
  const bool is_one = _count == 1;

  // Enqueue _count waiters
  _count = std::min<int>(_count, _old & c_waiter_mask);
  if (_count <= 0)
    return;

  // Try to lock the mutex
  const bool locked = mutex.lock_forced(_count);

  const int max_sig = atomic_op(m_value, [&](unsigned &value) {
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
      return impl_wake(mutex, is_one ? 1 : INT_MAX);
    }
  }
}
} // namespace orbis::utils
