#include "umtx.hpp"
#include "time.hpp"

orbis::ErrorCode orbis::umtx_lock_umtx(Thread *thread, ptr<umtx> umtx, ulong id,
                                       timespec *ts) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_unlock_umtx(Thread *thread, ptr<umtx> umtx,
                                         ulong id) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait(Thread *thread, ptr<void> addr, ulong id,
                                  timespec *timeout) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wake(Thread *thread, ptr<void> uaddr,
                                  sint n_wake) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_trylock_umutex(Thread *thread, ptr<umutex> m) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_lock_umutex(Thread *thread, ptr<umutex> m,
                                         timespec *timeout) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_unlock_umutex(Thread *thread, ptr<umutex> m) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_set_ceiling(Thread *thread, ptr<umutex> m,
                                         std::uint32_t ceiling,
                                         ptr<uint32_t> oldCeiling) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_cv_wait(Thread *thread, ptr<ucond> cv,
                                     ptr<umutex> m, timespec *ts,
                                     ulong wflags) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_cv_signal(Thread *thread, ptr<ucond> cv) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_cv_broadcast(Thread *thread, ptr<ucond> cv) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait_uint(Thread *thread, ptr<void> addr, ulong id,
                                       timespec *timeout) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_rw_rdlock(Thread *thread, ptr<void> obj,
                                       std::int64_t val, ptr<void> uaddr1,
                                       ptr<void> uaddr2) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_rw_wrlock(Thread *thread, ptr<void> obj,
                                       std::int64_t val, ptr<void> uaddr1,
                                       ptr<void> uaddr2) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_rw_unlock(Thread *thread, ptr<void> obj,
                                       std::int64_t val, ptr<void> uaddr1,
                                       ptr<void> uaddr2) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait_uint_private(Thread *thread, ptr<void> addr,
                                               ulong id, timespec *timeout) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wake_private(Thread *thread, ptr<void> uaddr,
                                          sint n_wake) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait_umutex(Thread *thread, ptr<umutex> m,
                                         timespec *timeout) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wake_umutex(Thread *thread, ptr<void> obj,
                                         std::int64_t val, ptr<void> uaddr1,
                                         ptr<void> uaddr2) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_sem_wait(Thread *thread, ptr<void> obj,
                                      std::int64_t val, ptr<void> uaddr1,
                                      ptr<void> uaddr2) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_sem_wake(Thread *thread, ptr<void> obj,
                                      std::int64_t val, ptr<void> uaddr1,
                                      ptr<void> uaddr2) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_nwake_private(Thread *thread, ptr<void> uaddrs,
                                           std::int64_t count) {
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wake2_umutex(Thread *thread, ptr<void> obj,
                                          std::int64_t val, ptr<void> uaddr1,
                                          ptr<void> uaddr2) {
  return ErrorCode::NOSYS;
}
