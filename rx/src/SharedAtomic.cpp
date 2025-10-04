#include "SharedAtomic.hpp"
using namespace rx;

#ifdef __linux__
#include <linux/futex.h>
#include <sys/syscall.h>

std::errc shared_atomic32::wait_impl(std::uint32_t oldValue,
                                     std::chrono::microseconds usec_timeout) {
  auto usec_timeout_count = usec_timeout.count();

  struct timespec timeout{};
  bool useTimeout = usec_timeout != std::chrono::microseconds::max();
  if (useTimeout) {
    timeout.tv_nsec = (usec_timeout_count % 1000'000) * 1000;
    timeout.tv_sec = (usec_timeout_count / 1000'000);
  }

  bool unblock = (!useTimeout || usec_timeout.count() > 1000) &&
                 g_scopedUnblock != nullptr;

  if (unblock) {
    if (!g_scopedUnblock(true)) {
      return std::errc::interrupted;
    }
  }

  int result = syscall(SYS_futex, this, FUTEX_WAIT, oldValue,
                       useTimeout ? &timeout : nullptr);

  auto errorCode = result < 0 ? static_cast<std::errc>(errno) : std::errc{};

  if (unblock) {
    if (!g_scopedUnblock(false)) {
      if (result < 0) {
        return std::errc::interrupted;
      }

      return {};
    }
  }

  if (result < 0) {
    if (errorCode == std::errc::interrupted) {
      return std::errc::resource_unavailable_try_again;
    }

    return errorCode;
  }

  return {};
}

int shared_atomic32::notify_n(int count) const {
  return syscall(SYS_futex, this, FUTEX_WAKE, count);
}
#elif defined(__APPLE__)
#include <limits>

#define UL_COMPARE_AND_WAIT 1
#define UL_UNFAIR_LOCK 2
#define UL_COMPARE_AND_WAIT_SHARED 3
#define UL_UNFAIR_LOCK64_SHARED 4
#define UL_COMPARE_AND_WAIT64 5
#define UL_COMPARE_AND_WAIT64_SHARED 6

#define ULF_WAKE_ALL 0x00000100
#define ULF_WAKE_THREAD 0x00000200
#define ULF_WAKE_ALLOW_NON_OWNER 0x00000400

#define ULF_WAIT_WORKQ_DATA_CONTENTION 0x00010000
#define ULF_WAIT_CANCEL_POINT 0x00020000
#define ULF_WAIT_ADAPTIVE_SPIN 0x00040000

#define ULF_NO_ERRNO 0x01000000

#define UL_OPCODE_MASK 0x000000FF
#define UL_FLAGS_MASK 0xFFFFFF00
#define ULF_GENERIC_MASK 0xFFFF0000

extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value,
                        uint32_t timeout);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);

std::errc shared_atomic32::wait_impl(std::uint32_t oldValue,
                                     std::chrono::microseconds usec_timeout) {
  bool useTimeout = usec_timeout != std::chrono::microseconds::max();
  bool unblock = (!useTimeout || usec_timeout.count() > 1000) &&
                 g_scopedUnblock != nullptr;

  if (unblock) {
    if (!g_scopedUnblock(true)) {
      return std::errc::interrupted;
    }
  }

  int result = __ulock_wait(UL_COMPARE_AND_WAIT_SHARED, (void *)this, oldValue,
                            usec_timeout.count());

  if (unblock) {
    if (!g_scopedUnblock(false)) {
      if (result < 0) {
        return std::errc::interrupted;
      }

      return {};
    }
  }

  if (result < 0) {
    return static_cast<std::errc>(errno);
  }

  return {};
}

int shared_atomic32::notify_n(int count) const {
  int result = 0;
  uint32_t operation = UL_COMPARE_AND_WAIT_SHARED | ULF_NO_ERRNO;
  if (count == 1) {
    result = __ulock_wake(operation, (void *)this, 0);
  } else if (count == std::numeric_limits<int>::max()) {
    result = __ulock_wake(ULF_WAKE_ALL | operation, (void *)this, 0);
  } else {
    for (int i = 0; i < count; ++i) {
      auto ret = __ulock_wake(operation, (void *)this, 0);
      if (ret != 0) {
        if (result == 0) {
          result = ret;
        }

        break;
      }

      result++;
    }
  }

  return result;
}
#elif defined(_WIN32)
#include <cmath>
#include <windows.h>

std::errc shared_atomic32::wait_impl(std::uint32_t oldValue,
                                     std::chrono::microseconds usec_timeout) {

  bool useTimeout = usec_timeout != std::chrono::microseconds::max();

  bool unblock = (!useTimeout || usec_timeout.count() > 1000) &&
                 g_scopedUnblock != nullptr;

  if (unblock) {
    if (!g_scopedUnblock(true)) {
      return std::errc::interrupted;
    }
  }

  BOOL result = WaitOnAddress(
      this, &oldValue, sizeof(std::uint32_t),
      useTimeout
          ? std::chrono::duration_cast<std::chrono::milliseconds>(usec_timeout)
                .count()
          : INFINITY);

  DWORD error = 0;
  if (!result) {
    error = GetLastError();
  } else {
    if (load(std::memory_order::relaxed) == oldValue) {
      error = ERROR_ALERTED; // dummy error
    }
  }

  if (unblock) {
    if (!g_scopedUnblock(false)) {
      if (result != TRUE) {
        return std::errc::interrupted;
      }

      return {};
    }
  }

  if (error == ERROR_TIMEOUT) {
    return std::errc::timed_out;
  }

  return std::errc::resource_unavailable_try_again;
}

int shared_atomic32::notify_n(int count) const {
  if (count == 1) {
    WakeByAddressSingle(const_cast<shared_atomic32 *>(this));
  } else if (count == std::numeric_limits<int>::max()) {
    WakeByAddressAll(const_cast<shared_atomic32 *>(this));
  } else {
    for (int i = 0; i < count; ++i) {
      WakeByAddressSingle(const_cast<shared_atomic32 *>(this));
    }
  }
}
#else
#error Unimplemented atomic for this platform
#endif
