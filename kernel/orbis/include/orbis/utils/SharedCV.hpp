#pragma once

#include "orbis/utils/SharedAtomic.hpp"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <orbis/utils/AtomicOp.hpp>
#include <orbis/utils/SharedMutex.hpp>
#include <system_error>

namespace orbis {
inline namespace utils {
// IPC-ready lightweight condition variable
class shared_cv final {
  enum : unsigned {
    c_waiter_mask = 0xffff,
    c_signal_mask = 0x7fff0000,
#ifdef ORBIS_HAS_FUTEX
    c_locked_mask = 0x80000000,
#endif
    c_signal_one = c_waiter_mask + 1,
  };

  shared_atomic32 m_value{0};

protected:
  // Increment waiter count
  unsigned add_waiter() noexcept {
    return m_value.op([](unsigned &value) -> unsigned {
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
  std::errc impl_wait(shared_mutex &mutex, unsigned _val,
                      std::uint64_t usec_timeout) noexcept;

  // Try to notify up to _count threads
  void impl_wake(shared_mutex &mutex, int _count) noexcept;

public:
  constexpr shared_cv() = default;

  std::errc
  wait(std::unique_lock<shared_mutex> &lock,
       std::chrono::microseconds timeout = std::chrono::microseconds::max()) {
    return wait(*lock.mutex(), timeout.count());
  }

  template <typename Rep, typename Period>
  std::errc wait(std::unique_lock<shared_mutex> &lock,
                 std::chrono::duration<Rep, Period> timeout) {
    return wait(
        lock,
        std::chrono::duration_cast<std::chrono::microseconds>(timeout).count());
  }

  std::errc wait(shared_mutex &mutex,
                 std::uint64_t usec_timeout = -1) noexcept {
    const unsigned _val = add_waiter();
    if (!_val) {
      return {};
    }

    mutex.unlock();
    return impl_wait(mutex, _val, usec_timeout);
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
