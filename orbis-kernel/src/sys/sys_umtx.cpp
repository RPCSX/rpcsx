#include "sys/sysproto.hpp"
#include "time.hpp"
#include "umtx.hpp"

static orbis::ErrorCode ureadTimespec(orbis::timespec &ts,
                                      orbis::ptr<orbis::timespec> addr) {
  ts = uread(addr);
  if (ts.sec < 0 || ts.nsec < 0 || ts.nsec > 1000000000) {
    return orbis::ErrorCode::INVAL;
  }

  return {};
}

orbis::SysResult orbis::sys__umtx_lock(Thread *thread, ptr<umtx> umtx) {
  return umtx_lock_umtx(thread, umtx, thread->tid, nullptr);
}
orbis::SysResult orbis::sys__umtx_unlock(Thread *thread, ptr<umtx> umtx) {
  return umtx_unlock_umtx(thread, umtx, thread->tid);
}
orbis::SysResult orbis::sys__umtx_op(Thread *thread, ptr<void> obj, sint op,
                                     ulong val, ptr<void> uaddr1,
                                     ptr<void> uaddr2) {
  switch (op) {
  case 0: {
    timespec *ts = nullptr;
    timespec timeout;
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }

    return umtx_lock_umtx(thread, (ptr<umtx>)obj, val, ts);
  }
  case 1:
    return umtx_unlock_umtx(thread, (ptr<umtx>)obj, val);
  case 2: {
    timespec *ts = nullptr;
    timespec timeout;
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }

    return umtx_wait(thread, obj, val, ts);
  }
  case 3:
    return umtx_wake(thread, obj, val);
  case 4:
    return umtx_trylock_umutex(thread, (ptr<umutex>)obj);
  case 5: {
    timespec *ts = nullptr;
    timespec timeout;
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }
    return umtx_lock_umutex(thread, (ptr<umutex>)obj, ts);
  }
  case 6:
    return umtx_unlock_umutex(thread, (ptr<umutex>)obj);
  case 7:
    return umtx_set_ceiling(thread, (ptr<umutex>)obj, val,
                            (ptr<uint32_t>)uaddr1);

  case 8: {
    timespec *ts = nullptr;
    timespec timeout;
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }

    return umtx_cv_wait(thread, (ptr<ucond>)obj, (ptr<umutex>)uaddr1, ts, val);
  }

  case 9:
    return umtx_cv_signal(thread, (ptr<ucond>)obj);
  case 10:
    return umtx_cv_broadcast(thread, (ptr<ucond>)obj);
  case 11: {
    timespec *ts = nullptr;
    timespec timeout;
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }

    return umtx_wait_uint(thread, obj, val, ts);
  }
  case 12:
    return umtx_rw_rdlock(thread, obj, val, uaddr1, uaddr2);
  case 13:
    return umtx_rw_wrlock(thread, obj, val, uaddr1, uaddr2);
  case 14:
    return umtx_rw_unlock(thread, obj, val, uaddr1, uaddr2);
  case 15: {
    timespec *ts = nullptr;
    timespec timeout;
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }

    return umtx_wait_uint_private(thread, obj, val, ts);
  }
  case 16:
    return umtx_wake_private(thread, obj, val);
  case 17: {
    timespec *ts = nullptr;
    timespec timeout;
    if (uaddr2 != nullptr) {
      auto result = ureadTimespec(timeout, (ptr<timespec>)uaddr2);
      if (result != ErrorCode{}) {
        return result;
      }

      ts = &timeout;
    }

    return umtx_wait_umutex(thread, (ptr<umutex>)obj, ts);
  }
  case 18:
    return umtx_wake_umutex(thread, obj, val, uaddr1, uaddr2);
  case 19:
    return umtx_sem_wait(thread, obj, val, uaddr1, uaddr2);
  case 20:
    return umtx_sem_wake(thread, obj, val, uaddr1, uaddr2);
  case 21:
    return umtx_nwake_private(thread, obj, val);
  case 22:
    return umtx_wake2_umutex(thread, obj, val, uaddr1, uaddr2);
  }

  return ErrorCode::INVAL;
}
