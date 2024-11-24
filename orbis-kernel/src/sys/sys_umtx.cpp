#include "orbis/utils/Logs.hpp"
#include "sys/sysproto.hpp"
#include "thread/Thread.hpp"
#include "time.hpp"
#include "umtx.hpp"
#include <chrono>

static orbis::ErrorCode ureadTimespec(orbis::timespec &ts,
                                      orbis::ptr<orbis::timespec> addr) {
  orbis::ErrorCode error = uread(ts, addr);
  if (error != orbis::ErrorCode{})
    return error;
  if (ts.sec < 0 || ts.nsec < 0 || ts.nsec > 1000000000) {
    return orbis::ErrorCode::INVAL;
  }

  return {};
}

orbis::SysResult orbis::sys__umtx_lock(Thread *thread, ptr<umtx> umtx) {
  ORBIS_LOG_TRACE(__FUNCTION__, umtx);
  if (reinterpret_cast<std::uintptr_t>(umtx) - 0x10000 > 0xff'fffe'ffff)
    return ErrorCode::FAULT;
  return umtx_lock_umtx(thread, umtx, thread->tid, -1);
}
orbis::SysResult orbis::sys__umtx_unlock(Thread *thread, ptr<umtx> umtx) {
  ORBIS_LOG_TRACE(__FUNCTION__, umtx);
  if (reinterpret_cast<std::uintptr_t>(umtx) - 0x10000 > 0xff'fffe'ffff)
    return ErrorCode::FAULT;
  return umtx_unlock_umtx(thread, umtx, thread->tid);
}
orbis::SysResult orbis::sys__umtx_op(Thread *thread, ptr<void> obj, sint op,
                                     ulong val, ptr<void> uaddr1,
                                     ptr<void> uaddr2) {
  ORBIS_LOG_TRACE(__FUNCTION__, obj, op, val, uaddr1, uaddr2);
  if (reinterpret_cast<std::uintptr_t>(obj) - 0x10000 > 0xff'fffe'ffff)
    return ErrorCode::FAULT;
  auto with_timeout = [&](auto op, bool loop = true) -> SysResult {
    timespec *ts = nullptr;
    timespec timeout{};
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }

    if (!ts) {
      if (!loop)
        return op(-1);
      while (true) {
        if (auto r = op(-1); r != ErrorCode::TIMEDOUT)
          return r;
      }
    } else {
      __uint128_t usec = timeout.sec;
      usec *= 1000'000;
      usec += (timeout.nsec + 999) / 1000;
      if (usec >= UINT64_MAX)
        usec = -2;

      if (!loop) {
        if (auto result = op(usec); result != ErrorCode::TIMEDOUT) {
          return result;
        }

        return SysResult::notAnError(ErrorCode::TIMEDOUT);
      }

      auto start = std::chrono::steady_clock::now();
      std::uint64_t udiff = 0;
      while (true) {
        if (auto r = op(usec - udiff); r != ErrorCode::TIMEDOUT)
          return r;
        udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
        if (udiff >= usec)
          return SysResult::notAnError(ErrorCode::TIMEDOUT);
      }
    }
  };

  switch (op) {
  case kUmtxOpLock: {
    return with_timeout([&](std::uint64_t ut) {
      return umtx_lock_umtx(thread, (ptr<umtx>)obj, val, ut);
    });
  }
  case kUmtxOpUnlock:
    return umtx_unlock_umtx(thread, (ptr<umtx>)obj, val);
  case kUmtxOpWait: {
    return with_timeout(
        [&](std::uint64_t ut) {
          return umtx_wait(thread, obj, val, ut, false, true);
        },
        false);
  }
  case kUmtxOpWake:
    return umtx_wake(thread, obj, val);
  case kUmtxOpMutexTrylock:
    return umtx_trylock_umutex(thread, (ptr<umutex>)obj);
  case kUmtxOpMutexLock: {
    return with_timeout([&](std::uint64_t ut) {
      return umtx_lock_umutex(thread, (ptr<umutex>)obj, ut);
    });
  }
  case kUmtxOpMutexUnock:
    return umtx_unlock_umutex(thread, (ptr<umutex>)obj);
  case kUmtxOpSetCeiling:
    return umtx_set_ceiling(thread, (ptr<umutex>)obj, val,
                            (ptr<uint32_t>)uaddr1);
  case kUmtxOpCvWait: {
    return with_timeout(
        [&](std::uint64_t ut) {
          return umtx_cv_wait(thread, (ptr<ucond>)obj, (ptr<umutex>)uaddr1, ut,
                              val);
        },
        false);
  }
  case kUmtxOpCvSignal:
    return umtx_cv_signal(thread, (ptr<ucond>)obj);
  case kUmtxOpCvBroadcast:
    return umtx_cv_broadcast(thread, (ptr<ucond>)obj);
  case kUmtxOpWaitUint: {
    return with_timeout(
        [&](std::uint64_t ut) {
          return umtx_wait(thread, obj, val, ut, true, true);
        },
        false);
  }
  case kUmtxOpRwRdLock:
    return with_timeout([&](std::uint64_t ut) {
      return umtx_rw_rdlock(thread, (ptr<urwlock>)obj, val, ut);
    });
  case kUmtxOpRwWrLock:
    return with_timeout([&](std::uint64_t ut) {
      return umtx_rw_wrlock(thread, (ptr<urwlock>)obj, ut);
    });
  case kUmtxOpRwUnlock:
    return umtx_rw_unlock(thread, (ptr<urwlock>)obj);
  case kUmtxOpWaitUintPrivate: {
    return with_timeout(
        [&](std::uint64_t ut) {
          return umtx_wait(thread, obj, val, ut, true, false);
        },
        false);
  }
  case kUmtxOpWakePrivate:
    return umtx_wake_private(thread, obj, val);
  case kUmtxOpMutexWait: {
    return with_timeout([&](std::uint64_t ut) {
      return umtx_wait_umutex(thread, (ptr<umutex>)obj, ut);
    });
  }
  case kUmtxOpMutexWake:
    return umtx_wake_umutex(thread, (ptr<umutex>)obj, 0);
  case kUmtxOpSemWait:
    return with_timeout(
        [&](std::uint64_t ut) {
          return umtx_sem_wait(thread, (ptr<usem>)obj, ut);
        },
        false);
  case kUmtxOpSemWake:
    return umtx_sem_wake(thread, (ptr<usem>)obj);
  case kUmtxOpNwakePrivate:
    return umtx_nwake_private(thread, (ptr<void *>)obj, val);
  case kUmtxOpMutexWake2:
    return umtx_wake2_umutex(thread, (orbis::ptr<orbis::umutex>)obj, val);
  case kUmtxOpMutexWake3:
    return umtx_wake3_umutex(thread, (orbis::ptr<orbis::umutex>)obj, val);
  }

  return ErrorCode::INVAL;
}
