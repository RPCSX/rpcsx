#pragma once

#include <cstdint>
#include <orbis/utils/AtomicOp.hpp>
#include <orbis/utils/SharedMutex.hpp>

namespace orbis {
inline namespace utils {
// IPC-ready lightweight condition variable
class shared_cv {
  enum : unsigned {
    c_waiter_mask = 0xffff,
    c_signal_mask = 0xffffffff & ~c_waiter_mask,
  };

  std::atomic<unsigned> m_value{0};

protected:
  // Increment waiter count
  unsigned add_waiter() noexcept {
    return atomic_op(m_value, [](unsigned &value) -> unsigned {
      if ((value & c_signal_mask) == c_signal_mask ||
          (value & c_waiter_mask) == c_waiter_mask) {
        // Signal or waiter overflow, return immediately
        return 0;
      }

      // Add waiter (c_waiter_mask)
      value += 1;
      return value;
    });
  }

  // Internal waiting function
  void impl_wait(shared_mutex &mutex, unsigned _old,
                 std::uint64_t usec_timeout) noexcept;

  // Try to notify up to _count threads
  void impl_wake(shared_mutex &mutex, int _count) noexcept;

public:
  constexpr shared_cv() = default;

  void wait(shared_mutex &mutex, std::uint64_t usec_timeout = -1) noexcept {
    const unsigned _old = add_waiter();
    if (!_old) {
      return;
    }

    mutex.unlock();
    impl_wait(mutex, _old, usec_timeout);
  }

  // Wake one thread
  void notify_one(shared_mutex &mutex) noexcept {
    if (m_value) {
      impl_wake(mutex, 1);
    }
  }

  // Wake all threads
  void notify_all(shared_mutex &mutex) noexcept {
    if (m_value) {
      impl_wake(mutex, INT_MAX);
    }
  }
};
} // namespace utils
} // namespace orbis
