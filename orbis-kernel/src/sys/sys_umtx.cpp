#include "sys/sysproto.hpp"
#include "umtx.hpp"

orbis::SysResult orbis::sys__umtx_lock(Thread *thread, ptr<struct umtx> umtx) {
  return umtx_lock_umtx(thread, umtx, 0, 0, 0);
}
orbis::SysResult orbis::sys__umtx_unlock(Thread *thread,
                                         ptr<struct umtx> umtx) {
  return umtx_lock_umtx(thread, umtx, thread->tid, 0, 0);
}
orbis::SysResult orbis::sys__umtx_op(Thread *thread, ptr<void> obj, sint op,
                                     ulong val, ptr<void> uaddr1,
                                     ptr<void> uaddr2) {
  switch (op) {
  case 0:
    return umtx_lock_umtx(thread, obj, val, uaddr1, uaddr2);
  case 1:
    return umtx_unlock_umtx(thread, obj, val, uaddr1, uaddr2);
  case 2:
    return umtx_wait(thread, obj, val, uaddr1, uaddr2);
  case 3:
    return umtx_wake(thread, obj, val, uaddr1, uaddr2);
  case 4:
    return umtx_trylock_umutex(thread, obj, val, uaddr1, uaddr2);
  case 5:
    return umtx_lock_umutex(thread, obj, val, uaddr1, uaddr2);
  case 6:
    return umtx_unlock_umutex(thread, obj, val, uaddr1, uaddr2);
  case 7:
    return umtx_set_ceiling(thread, obj, val, uaddr1, uaddr2);
  case 8:
    return umtx_cv_wait(thread, obj, val, uaddr1, uaddr2);
  case 9:
    return umtx_cv_signal(thread, obj, val, uaddr1, uaddr2);
  case 10:
    return umtx_cv_broadcast(thread, obj, val, uaddr1, uaddr2);
  case 11:
    return umtx_wait_uint(thread, obj, val, uaddr1, uaddr2);
  case 12:
    return umtx_rw_rdlock(thread, obj, val, uaddr1, uaddr2);
  case 13:
    return umtx_rw_wrlock(thread, obj, val, uaddr1, uaddr2);
  case 14:
    return umtx_rw_unlock(thread, obj, val, uaddr1, uaddr2);
  case 15:
    return umtx_wait_uint_private(thread, obj, val, uaddr1, uaddr2);
  case 16:
    return umtx_wake_private(thread, obj, val, uaddr1, uaddr2);
  case 17:
    return umtx_wait_umutex(thread, obj, val, uaddr1, uaddr2);
  case 18:
    return umtx_wake_umutex(thread, obj, val, uaddr1, uaddr2);
  case 19:
    return umtx_sem_wait(thread, obj, val, uaddr1, uaddr2);
  case 20:
    return umtx_sem_wake(thread, obj, val, uaddr1, uaddr2);
  case 21:
    return umtx_nwake_private(thread, obj, val, uaddr1, uaddr2);
  case 22:
    return umtx_wake2_umutex(thread, obj, val, uaddr1, uaddr2);
  }

  return ErrorCode::INVAL;
}
