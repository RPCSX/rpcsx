#pragma once

#include "error/ErrorCode.hpp"
#include "orbis-config.hpp"
#include "thread/types.hpp"
#include "time.hpp"
#include <atomic>
#include <limits>

namespace orbis {
inline constexpr ulong kUmtxUnowned = 0;
inline constexpr ulong kUmtxContested = std::numeric_limits<slong>::min();

inline constexpr auto kUsyncProcessShared = 1;

inline constexpr auto kUmutexUnowned = 0;
inline constexpr auto kUmutexContested = 0x80000000;
inline constexpr auto kUmutexErrorCheck = 2;
inline constexpr auto kUmutexPrioInherit = 4;
inline constexpr auto kUmutexPrioProtect = 8;

inline constexpr auto kUrwLockPreferReader = 2;
inline constexpr auto kUrwLockWriteOwner = 0x80000000;
inline constexpr auto kUrwLockWriteWaiters = 0x40000000;
inline constexpr auto kUrwLockReadWaiters = 0x20000000;
inline constexpr auto kUrwLockMaxReaders = 0x1fffffff;

inline constexpr auto kSemNamed = 2;

struct umtx {
  std::atomic<ulong> owner; // Owner of the mutex
};

struct umutex {
  std::atomic<lwpid_t> owner; // Owner of the mutex
  uint32_t flags;             // Flags of the mutex
  uint32_t ceilings[2];       // Priority protect ceiling
  uint32_t spare[4];
};

struct ucond {
  std::atomic<uint32_t> has_waiters; // Has waiters in kernel
  uint32_t flags;                    // Flags of the condition variable
  uint32_t clockid;                  // Clock id
  uint32_t spare[1];                 // Spare space
};

struct urwlock {
  std::atomic<int32_t> state;
  uint32_t flags;
  uint32_t blocked_readers;
  uint32_t blocked_writers;
  uint32_t spare[4];
};

struct usem {
  std::atomic<uint32_t> has_waiters;
  std::atomic<uint32_t> count;
  uint32_t flags;
};

struct Thread;
ErrorCode umtx_lock_umtx(Thread *thread, ptr<umtx> umtx, ulong id,
                         std::uint64_t ut);
ErrorCode umtx_unlock_umtx(Thread *thread, ptr<umtx> umtx, ulong id);
ErrorCode umtx_wait(Thread *thread, ptr<void> addr, ulong id, std::uint64_t ut);
ErrorCode umtx_wake(Thread *thread, ptr<void> addr, sint n_wake);
ErrorCode umtx_trylock_umutex(Thread *thread, ptr<umutex> m);
ErrorCode umtx_lock_umutex(Thread *thread, ptr<umutex> m, std::uint64_t ut);
ErrorCode umtx_unlock_umutex(Thread *thread, ptr<umutex> m);
ErrorCode umtx_set_ceiling(Thread *thread, ptr<umutex> m, std::uint32_t ceiling,
                           ptr<uint32_t> oldCeiling);
ErrorCode umtx_cv_wait(Thread *thread, ptr<ucond> cv, ptr<umutex> m,
                       std::uint64_t ut, ulong wflags);
ErrorCode umtx_cv_signal(Thread *thread, ptr<ucond> cv);
ErrorCode umtx_cv_broadcast(Thread *thread, ptr<ucond> cv);
ErrorCode umtx_wait_uint(Thread *thread, ptr<void> addr, ulong id,
                         std::uint64_t ut);
ErrorCode umtx_rw_rdlock(Thread *thread, ptr<void> obj, std::int64_t val,
                         ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_rw_wrlock(Thread *thread, ptr<void> obj, std::int64_t val,
                         ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_rw_unlock(Thread *thread, ptr<void> obj, std::int64_t val,
                         ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wait_uint_private(Thread *thread, ptr<void> addr, ulong id,
                                 std::uint64_t ut);
ErrorCode umtx_wake_private(Thread *thread, ptr<void> uaddr, sint n_wake);
ErrorCode umtx_wait_umutex(Thread *thread, ptr<umutex> m, std::uint64_t ut);
ErrorCode umtx_wake_umutex(Thread *thread, ptr<umutex> m);
ErrorCode umtx_sem_wait(Thread *thread, ptr<void> obj, std::int64_t val,
                        ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_sem_wake(Thread *thread, ptr<void> obj, std::int64_t val,
                        ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_nwake_private(Thread *thread, ptr<void> uaddrs,
                             std::int64_t count);
ErrorCode umtx_wake2_umutex(Thread *thread, ptr<void> obj, std::int64_t val,
                            ptr<void> uaddr1, ptr<void> uaddr2);
} // namespace orbis