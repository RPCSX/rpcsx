#pragma once

#include "error/ErrorCode.hpp"
#include "orbis-config.hpp"

namespace orbis {
struct Thread;
ErrorCode umtx_lock_umtx(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_unlock_umtx(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wait(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wake(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_trylock_umutex(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_lock_umutex(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_unlock_umutex(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_set_ceiling(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_cv_wait(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_cv_signal(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_cv_broadcast(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wait_uint(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_rw_rdlock(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_rw_wrlock(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_rw_unlock(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wait_uint_private(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wake_private(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wait_umutex(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wake_umutex(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_sem_wait(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_sem_wake(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_nwake_private(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
ErrorCode umtx_wake2_umutex(Thread *thread, ptr<void> obj, std::int64_t val, ptr<void> uaddr1, ptr<void> uaddr2);
}