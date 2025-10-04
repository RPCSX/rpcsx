#include "utils/SharedMutex.hpp"
#include "utils/Logs.hpp"
#include <syscall.h>
#include <unistd.h>
#include <xmmintrin.h>

static void busy_wait(unsigned long long cycles = 3000) {
  const auto stop = __builtin_ia32_rdtsc() + cycles;
  do
    _mm_pause();
  while (__builtin_ia32_rdtsc() < stop);
}

namespace orbis::utils {
void shared_mutex::impl_lock_shared(unsigned val) {
  if (val >= c_err)
    std::abort(); // "shared_mutex underflow"

  // Try to steal the notification bit
  unsigned _old = val;
  if (val & c_sig && m_value.compare_exchange_strong(_old, val - c_sig + 1)) {
    return;
  }

  for (int i = 0; i < 10; i++) {
    if (try_lock_shared()) {
      return;
    }

    unsigned old = m_value;

    if (old & c_sig && m_value.compare_exchange_strong(old, old - c_sig + 1)) {
      return;
    }

    busy_wait();
  }

  // Acquire writer lock and downgrade
  const unsigned old = m_value.fetch_add(c_one);

  if (old == 0) {
    lock_downgrade();
    return;
  }

  if ((old % c_sig) + c_one >= c_sig)
    std::abort(); // "shared_mutex overflow"

  while (impl_wait() != std::errc{}) {
  }
  lock_downgrade();
}
void shared_mutex::impl_unlock_shared(unsigned old) {
  if (old - 1 >= c_err)
    std::abort(); // "shared_mutex underflow"

  // Check reader count, notify the writer if necessary
  if ((old - 1) % c_one == 0) {
    impl_signal();
  }
}
std::errc shared_mutex::impl_wait() {
  while (true) {
    const auto [old, ok] = m_value.fetch_op([](unsigned &value) {
      if (value >= c_sig) {
        value -= c_sig;
        return true;
      }

      return false;
    });

    if (ok) {
      break;
    }

    auto result = m_value.wait(old);
    if (result == std::errc::interrupted) {
      return result;
    }
  }

  return {};
}
void shared_mutex::impl_signal() {
  m_value += c_sig;
  m_value.notify_one();
}
void shared_mutex::impl_lock(unsigned val) {
  if (val >= c_err)
    std::abort(); // "shared_mutex underflow"

  // Try to steal the notification bit
  unsigned _old = val;
  if (val & c_sig &&
      m_value.compare_exchange_strong(_old, val - c_sig + c_one)) {
    return;
  }

  for (int i = 0; i < 10; i++) {
    busy_wait();

    unsigned old = m_value;

    if (!old && try_lock()) {
      return;
    }

    if (old & c_sig &&
        m_value.compare_exchange_strong(old, old - c_sig + c_one)) {
      return;
    }
  }

  const unsigned old = m_value.fetch_add(c_one);

  if (old == 0) {
    return;
  }

  if ((old % c_sig) + c_one >= c_sig)
    std::abort(); // "shared_mutex overflow"
  while (impl_wait() != std::errc{}) {
  }
}
void shared_mutex::impl_unlock(unsigned old) {
  if (old - c_one >= c_err)
    std::abort(); // "shared_mutex underflow"

  // 1) Notify the next writer if necessary
  // 2) Notify all readers otherwise if necessary (currently indistinguishable
  // from writers)
  if (old - c_one) {
    impl_signal();
  }
}
void shared_mutex::impl_lock_upgrade() {
  for (int i = 0; i < 10; i++) {
    busy_wait();

    if (try_lock_upgrade()) {
      return;
    }
  }

  // Convert to writer lock
  const unsigned old = m_value.fetch_add(c_one - 1);

  if ((old % c_sig) + c_one - 1 >= c_sig)
    std::abort(); // "shared_mutex overflow"

  if (old % c_one == 1) {
    return;
  }

  while (impl_wait() != std::errc{}) {
  }
}
bool shared_mutex::lock_forced(int count) {
  if (count == 0)
    return false;
  if (count > 0) {
    // Lock
    return m_value.op([&](std::uint32_t &v) {
      if (v & c_sig) {
        v -= c_sig;
        v += c_one * count;
        return true;
      }

      bool firstLock = v == 0;
      v += c_one * count;
      return firstLock;
    });
  }

  // Remove waiters
  m_value.fetch_add(c_one * count);
  return true;
}
} // namespace orbis::utils
