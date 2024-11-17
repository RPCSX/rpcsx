#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <system_error>
#include <thread>
#include <type_traits>

namespace orbis {
inline void yield() { std::this_thread::yield(); }
inline void relax() {
#if defined(__GNUC__) && (defined __i386__ || defined __x86_64__)
  __builtin_ia32_pause();
#else
  yield();
#endif
}

static constexpr auto kRelaxSpinCount = 12;
static constexpr auto kSpinCount = 16;

inline namespace utils {
inline thread_local void (*g_scopedUnblock)(bool) = nullptr;

bool try_spin_wait(auto &&pred) {
  for (std::size_t i = 0; i < kSpinCount; ++i) {
    if (pred()) {
      return true;
    }

    if (i < kRelaxSpinCount) {
      relax();
    } else {
      yield();
    }
  }

  return false;
}

bool spin_wait(auto &&pred, auto &&spinCond) {
  if (try_spin_wait(pred)) {
    return true;
  }

  while (spinCond()) {
    if (pred()) {
      return true;
    }
  }

  return false;
}

struct shared_atomic32 : std::atomic<std::uint32_t> {
  using atomic::atomic;
  using atomic::operator=;

  template <typename Clock, typename Dur>
  std::errc wait(std::uint32_t oldValue,
                 std::chrono::time_point<Clock, Dur> timeout) {
    if (try_spin_wait(
            [&] { return load(std::memory_order::acquire) != oldValue; })) {
      return {};
    }

    auto now = Clock::now();

    if (timeout < now) {
      return std::errc::timed_out;
    }

    return wait_impl(
        oldValue,
        std::chrono::duration_cast<std::chrono::microseconds>(timeout - now));
  }

  std::errc wait(std::uint32_t oldValue,
                 std::chrono::microseconds usec_timeout) {
    return wait_impl(oldValue, usec_timeout);
  }

  std::errc wait(std::uint32_t oldValue) {
    if (try_spin_wait(
            [&] { return load(std::memory_order::acquire) != oldValue; })) {
      return {};
    }

    return wait_impl(oldValue);
  }

  auto wait(auto &fn) -> decltype(fn(std::declval<std::uint32_t &>())) {
    while (true) {
      std::uint32_t lastValue;
      if (try_spin_wait([&] {
            lastValue = load(std::memory_order::acquire);
            return fn(lastValue);
          })) {
        return;
      }

      while (wait_impl(lastValue) != std::errc{}) {
      }
    }
  }

  int notify_one() const { return notify_n(1); }
  int notify_all() const { return notify_n(std::numeric_limits<int>::max()); }

  int notify_n(int count) const;

  // Atomic operation; returns old value, or pair of old value and return value
  // (cancel op if evaluates to false)
  template <typename F, typename RT = std::invoke_result_t<F, std::uint32_t &>>
  std::conditional_t<std::is_void_v<RT>, std::uint32_t,
                     std::pair<std::uint32_t, RT>>
  fetch_op(F &&func) {
    std::uint32_t _new;
    std::uint32_t old = load(std::memory_order::relaxed);
    while (true) {
      _new = old;
      if constexpr (std::is_void_v<RT>) {
        std::invoke(std::forward<F>(func), _new);
        if (compare_exchange_strong(old, _new)) [[likely]] {
          return old;
        }
      } else {
        RT ret = std::invoke(std::forward<F>(func), _new);
        if (!ret || compare_exchange_strong(old, _new)) [[likely]] {
          return {old, std::move(ret)};
        }
      }
    }
  }

  // Atomic operation; returns function result value
  template <typename F, typename RT = std::invoke_result_t<F, std::uint32_t &>>
  RT op(F &&func) {
    std::uint32_t _new;
    std::uint32_t old = load(std::memory_order::relaxed);

    while (true) {
      _new = old;
      if constexpr (std::is_void_v<RT>) {
        std::invoke(std::forward<F>(func), _new);
        if (compare_exchange_strong(old, _new)) [[likely]] {
          return;
        }
      } else {
        RT result = std::invoke(std::forward<F>(func), _new);
        if (compare_exchange_strong(old, _new)) [[likely]] {
          return result;
        }
      }
    }
  }

private:
  [[nodiscard]] std::errc wait_impl(std::uint32_t oldValue,
                                    std::chrono::microseconds usec_timeout =
                                        std::chrono::microseconds::max());
};
} // namespace utils
} // namespace orbis
