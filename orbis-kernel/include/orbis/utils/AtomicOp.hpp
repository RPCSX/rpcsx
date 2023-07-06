#pragma once

#include <atomic>
#include <functional>
#include <utility>

namespace orbis {
inline namespace utils {
// Atomic operation; returns old value, or pair of old value and return value
// (cancel op if evaluates to false)
template <typename T, typename F, typename RT = std::invoke_result_t<F, T &>>
inline std::conditional_t<std::is_void_v<RT>, T, std::pair<T, RT>>
atomic_fetch_op(std::atomic<T> &v, F func) {
  T _new, old = v.load();
  while (true) {
    _new = old;
    if constexpr (std::is_void_v<RT>) {
      std::invoke(func, _new);
      if (v.compare_exchange_strong(old, _new)) [[likely]] {
        return old;
      }
    } else {
      RT ret = std::invoke(func, _new);
      if (!ret || v.compare_exchange_strong(old, _new)) [[likely]] {
        return {old, std::move(ret)};
      }
    }
  }
}

// Atomic operation; returns function result value, function is the lambda
template <typename T, typename F, typename RT = std::invoke_result_t<F, T &>>
inline RT atomic_op(std::atomic<T> &v, F func) {
  T _new, old = v.load();
  while (true) {
    _new = old;
    if constexpr (std::is_void_v<RT>) {
      std::invoke(func, _new);
      if (v.compare_exchange_strong(old, _new)) [[likely]] {
        return;
      }
    } else {
      RT result = std::invoke(func, _new);
      if (v.compare_exchange_strong(old, _new)) [[likely]] {
        return result;
      }
    }
  }
}

#if defined(__ATOMIC_HLE_ACQUIRE) && defined(__ATOMIC_HLE_RELEASE)
static constexpr int s_hle_ack = __ATOMIC_SEQ_CST | __ATOMIC_HLE_ACQUIRE;
static constexpr int s_hle_rel = __ATOMIC_SEQ_CST | __ATOMIC_HLE_RELEASE;
#else
static constexpr int s_hle_ack = __ATOMIC_SEQ_CST;
static constexpr int s_hle_rel = __ATOMIC_SEQ_CST;
#endif

template <typename T>
inline bool compare_exchange_hle_acq(std::atomic<T> &dest, T &comp, T exch) {
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  static_assert(std::atomic<T>::is_always_lock_free);
  return __atomic_compare_exchange(reinterpret_cast<T *>(&dest), &comp, &exch,
                                   false, s_hle_ack, s_hle_ack);
}

template <typename T>
inline T fetch_add_hle_rel(std::atomic<T> &dest, T value) {
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  static_assert(std::atomic<T>::is_always_lock_free);
  return __atomic_fetch_add(reinterpret_cast<T *>(&dest), value, s_hle_rel);
}
} // namespace utils
} // namespace orbis
