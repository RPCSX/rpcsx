#include "utils/SharedMutex.hpp"
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
    std::abort; // "shared_mutex overflow"
  impl_wait();
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
void shared_mutex::impl_wait() {
  pthread_mutex_lock(&m_mutex); // Acquire the mutex

  while (true) {
    if (m_value >= c_sig) {
      m_value -= c_sig;
      pthread_mutex_unlock(&m_mutex); // Release the mutex
      break;
    }
    else {
      pthread_cond_wait(&m_cond, &m_mutex); // Wait on the condition variable
    }
  }

  pthread_mutex_unlock(&m_mutex); // Release the mutex
}
void shared_mutex::impl_signal() {
    pthread_mutex_lock(&m_mutex);  // Acquire the mutex

    m_value += c_sig;
    pthread_cond_signal(&m_cond);  // Signal the condition variable

    pthread_mutex_unlock(&m_mutex);  // Release the mutex
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
  impl_wait();
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

  impl_wait();
}
} // namespace orbis::utils
