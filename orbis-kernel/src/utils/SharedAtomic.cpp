#include "utils/SharedAtomic.hpp"
using namespace orbis;

#ifdef ORBIS_HAS_FUTEX
#include <linux/futex.h>

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
    g_scopedUnblock(true);
  }

  int result = syscall(SYS_futex, this, FUTEX_WAIT, oldValue,
                       useTimeout ? &timeout : nullptr);

  if (unblock) {
    g_scopedUnblock(false);
  }

  if (result < 0) {
    return static_cast<std::errc>(errno);
  }

  return {};
}

int shared_atomic32::notify_n(int count) const {
  return syscall(SYS_futex, this, FUTEX_WAKE, count);
}
#elif defined(ORBIS_HAS_ULOCK)
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
  int result = __ulock_wait(UL_COMPARE_AND_WAIT_SHARED, (void *)this, oldValue,
                            usec_timeout.count());

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
#else
#error Unimplemented atomic for this platform
#endif
