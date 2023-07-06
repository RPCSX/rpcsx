#include "orbis/utils/SharedCV.hpp"
#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>

namespace orbis::utils {
void shared_cv::impl_wait(shared_mutex &mutex, unsigned _old,
                          std::uint64_t usec_timeout) noexcept {
  // Not supposed to fail
  if (!_old)
    std::abort();

  // Wait with timeout
  struct timespec timeout {};
  timeout.tv_nsec = (usec_timeout % 1000'000) * 1000;
  timeout.tv_sec = (usec_timeout / 1000'000);
  syscall(SYS_futex, &m_value, FUTEX_WAIT, _old,
          usec_timeout + 1 ? &timeout : nullptr, 0, 0);

  // Cleanup
  const auto old = atomic_fetch_op(m_value, [](unsigned &value) {
    // Remove waiter (c_waiter_mask)
    value -= 1;

    // Try to remove signal
    if (value & c_signal_mask)
      value -= c_signal_mask & (0 - c_signal_mask);
  });

  // Lock is already acquired
  if (old & c_signal_mask) {
    return;
  }

  mutex.lock();
}

void shared_cv::impl_wake(shared_mutex &mutex, int _count) noexcept {
  unsigned _old = m_value.load();
  const bool is_one = _count == 1;

  // Enqueue _count waiters
  _count = std::min<int>(_count, _old & c_waiter_mask);
  if (_count <= 0)
    return;

  // Try to lock the mutex
  unsigned _m_locks = mutex.lock_forced();

  const int max_sig = atomic_op(m_value, [&](unsigned &value) {
    // Verify the number of waiters
    int max_sig = std::min<int>(_count, value & c_waiter_mask);

    // Add lock signal (mutex was immediately locked)
    if (_m_locks == 0)
      value += c_signal_mask & (0 - c_signal_mask);
    _old = value;
    return max_sig;
  });

  if (max_sig < _count) {
    _count = max_sig;
  }

  if (_count) {
    // Wake up one thread + requeue remaining waiters
    unsigned do_wake = _m_locks == 0;
    if (auto r = syscall(SYS_futex, &m_value, FUTEX_CMP_REQUEUE, do_wake,
                         &mutex, _count - do_wake, _old);
        r > 0) {
      if (mutex.is_free()) // Avoid deadlock (TODO: proper fix?)
        syscall(SYS_futex, &mutex, FUTEX_WAKE, 1, nullptr, 0, 0);
      if (!is_one) // Keep awaking waiters
        return impl_wake(mutex, INT_MAX);
    } else if (r == EAGAIN) {
      // Retry if something has changed (TODO: is it necessary?)
      return impl_wake(mutex, is_one ? 1 : INT_MAX);
    } else if (r < 0)
      std::abort(); // Unknown error
  }
}
} // namespace orbis::utils
