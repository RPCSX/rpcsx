#pragma once

#include <atomic>
#include <mutex>
#include <orbis/utils/AtomicOp.hpp>

namespace orbis {
inline namespace utils {
// IPC-ready shared mutex, using only writer lock is recommended
struct shared_mutex final {
  enum : unsigned {
    c_one = 1u << 14, // Fixed-point 1.0 value (one writer)
    c_sig = 1u << 30,
    c_err = 1u << 31,
  };

  std::atomic<unsigned> m_value{};

  void impl_lock_shared(unsigned val);
  void impl_unlock_shared(unsigned old);
  void impl_wait();
  void impl_signal();
  void impl_lock(unsigned val);
  void impl_unlock(unsigned old);
  void impl_lock_upgrade();

public:
  constexpr shared_mutex() = default;

  bool try_lock_shared() {
    // Conditional increment
    unsigned value = m_value.load();
    return value < c_one - 1 &&
           m_value.compare_exchange_strong(value, value + 1);
  }

  // Lock with HLE acquire hint
  void lock_shared() {
    unsigned value = m_value.load();
    if (value < c_one - 1) [[likely]] {
      unsigned old = value;
      if (compare_exchange_hle_acq(m_value, old, value + 1)) [[likely]] {
        return;
      }
    }

    impl_lock_shared(value + 1);
  }

  // Unlock with HLE release hint
  void unlock_shared() {
    const unsigned value = fetch_add_hle_rel(m_value, -1u);
    if (value >= c_one) [[unlikely]] {
      impl_unlock_shared(value);
    }
  }

  bool try_lock() {
    unsigned value = 0;
    return m_value.compare_exchange_strong(value, c_one);
  }

  // Lock with HLE acquire hint
  void lock() {
    unsigned value = 0;
    if (!compare_exchange_hle_acq(m_value, value, +c_one)) [[unlikely]] {
      impl_lock(value);
    }
  }

  // Unlock with HLE release hint
  void unlock() {
    const unsigned value = fetch_add_hle_rel(m_value, 0u - c_one);
    if (value != c_one) [[unlikely]] {
      impl_unlock(value);
    }
  }

  bool try_lock_upgrade() {
    unsigned value = m_value.load();

    // Conditional increment, try to convert a single reader into a writer,
    // ignoring other writers
    return (value + c_one - 1) % c_one == 0 &&
           m_value.compare_exchange_strong(value, value + c_one - 1);
  }

  void lock_upgrade() {
    if (!try_lock_upgrade()) [[unlikely]] {
      impl_lock_upgrade();
    }
  }

  void lock_downgrade() {
    // Convert to reader lock (can result in broken state)
    m_value -= c_one - 1;
  }

  // Check whether can immediately obtain an exclusive (writer) lock
  bool is_free() const { return m_value.load() == 0; }

  // Check whether can immediately obtain a shared (reader) lock
  bool is_lockable() const { return m_value.load() < c_one - 1; }
};

// Simplified shared (reader) lock implementation.
class reader_lock final {
  shared_mutex &m_mutex;
  bool m_upgraded = false;

public:
  reader_lock(const reader_lock &) = delete;
  reader_lock &operator=(const reader_lock &) = delete;
  explicit reader_lock(shared_mutex &mutex) : m_mutex(mutex) {
    m_mutex.lock_shared();
  }

  // One-way lock upgrade; note that the observed state could have been changed
  void upgrade() {
    if (!m_upgraded) {
      m_mutex.lock_upgrade();
      m_upgraded = true;
    }
  }

  // Try to upgrade; if it succeeds, the observed state has NOT been changed
  bool try_upgrade() {
    return m_upgraded || (m_upgraded = m_mutex.try_lock_upgrade());
  }

  ~reader_lock() { m_upgraded ? m_mutex.unlock() : m_mutex.unlock_shared(); }
};

class writer_lock final {
  shared_mutex &m_mutex;

public:
  writer_lock(const writer_lock &) = delete;
  writer_lock &operator=(const writer_lock &) = delete;
  explicit writer_lock(shared_mutex &mutex) : m_mutex(mutex) { m_mutex.lock(); }
  ~writer_lock() { m_mutex.unlock(); }
};
} // namespace utils
} // namespace orbis
