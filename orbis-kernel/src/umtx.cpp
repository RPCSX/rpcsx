#include "umtx.hpp"
#include "error.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/thread.hpp"
#include "orbis/utils/Logs.hpp"
#include <limits>

namespace orbis {
std::pair<const UmtxKey, UmtxCond> *UmtxChain::enqueue(UmtxKey &key,
                                                       Thread *thr) {
  if (!spare_queue.empty()) {
    auto node = spare_queue.extract(spare_queue.begin());
    node.key() = key;
    node.mapped().thr = thr;
    return &*sleep_queue.insert(std::move(node));
  }
  return &*sleep_queue.emplace(key, thr);
}

void UmtxChain::erase(std::pair<const UmtxKey, UmtxCond> *obj) {
  for (auto [it, e] = sleep_queue.equal_range(obj->first); it != e; it++) {
    if (&*it == obj) {
      erase(it);
      return;
    }
  }
}

UmtxChain::queue_type::iterator UmtxChain::erase(queue_type::iterator it) {
  auto next = std::next(it);
  auto node = sleep_queue.extract(it);
  node.key() = {};
  spare_queue.insert(spare_queue.begin(), std::move(node));
  return next;
}

uint UmtxChain::notify_n(const UmtxKey &key, sint count) {
  auto it = sleep_queue.find(key);
  if (it == sleep_queue.end())
    return 0;

  uint n = 0;
  while (count > 0) {
    it->second.thr = nullptr;
    it->second.cv.notify_all(mtx);
    it = erase(it);

    n++;
    count--;

    if (it == sleep_queue.end()) {
      break;
    }
  }

  return n;
}

uint UmtxChain::notify_one(const UmtxKey &key) { return notify_n(key, 1); }

uint UmtxChain::notify_all(const UmtxKey &key) {
  return notify_n(key, std::numeric_limits<sint>::max());
}
} // namespace orbis

orbis::ErrorCode orbis::umtx_lock_umtx(Thread *thread, ptr<umtx> umtx, ulong id,
                                       std::uint64_t ut) {
  ORBIS_LOG_TODO(__FUNCTION__, thread->tid, umtx, id, ut);
  std::abort();
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_unlock_umtx(Thread *thread, ptr<umtx> umtx,
                                         ulong id) {
  ORBIS_LOG_TODO(__FUNCTION__, thread->tid, umtx, id);
  std::abort();
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait(Thread *thread, ptr<void> addr, ulong id,
                                  std::uint64_t ut, bool is32, bool ipc) {
  ORBIS_LOG_NOTICE(__FUNCTION__, thread->tid, addr, id, ut, is32);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, ipc, addr);
  auto node = chain.enqueue(key, thread);
  ErrorCode result = {};
  ulong val = 0;
  if (is32)
    val = reinterpret_cast<ptr<std::atomic<uint>>>(addr)->load();
  else
    val = reinterpret_cast<ptr<std::atomic<ulong>>>(addr)->load();
  if (val == id) {
    if (ut + 1 == 0) {
      while (true) {
        orbis::scoped_unblock unblock;
        result = orbis::toErrorCode(node->second.cv.wait(chain.mtx));
        if (result != ErrorCode{} || node->second.thr != thread)
          break;
      }
    } else {
      auto start = std::chrono::steady_clock::now();
      std::uint64_t udiff = 0;
      while (true) {
        orbis::scoped_unblock unblock;
        result =
            orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut - udiff));
        if (node->second.thr != thread)
          break;
        udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
        if (udiff >= ut) {
          result = ErrorCode::TIMEDOUT;
          break;
        }
        if (result != ErrorCode{}) {
          break;
        }
      }
    }
  }

  ORBIS_LOG_NOTICE(__FUNCTION__, "wakeup", thread->tid, addr);
  if (node->second.thr == thread)
    chain.erase(node);
  return result;
}

orbis::ErrorCode orbis::umtx_wake(Thread *thread, ptr<void> addr, sint n_wake) {
  ORBIS_LOG_NOTICE(__FUNCTION__, thread->tid, addr, n_wake);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, true, addr);
  if (key.pid == 0) {
    // IPC workaround (TODO)
    chain.notify_all(key);
    return {};
  }
  chain.notify_n(key, n_wake);
  return {};
}

namespace orbis {
enum class umutex_lock_mode {
  lock,
  try_,
  wait,
};
template <>
void log_class_string<umutex_lock_mode>::format(std::string &out,
                                                const void *arg) {
  switch (get_object(arg)) {
  case umutex_lock_mode::lock:
    out += "lock";
    break;
  case umutex_lock_mode::try_:
    out += "try";
    break;
  case umutex_lock_mode::wait:
    out += "wait";
    break;
  }
}
static ErrorCode do_lock_normal(Thread *thread, ptr<umutex> m, uint flags,
                                std::uint64_t ut, umutex_lock_mode mode) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, m, flags, ut, mode);

  ErrorCode error = {};
  while (true) {
    int owner = m->owner.load(std::memory_order_acquire);
    if (mode == umutex_lock_mode::wait) {
      if (owner == kUmutexUnowned || owner == kUmutexContested)
        return {};
    } else {
      owner = kUmutexUnowned;
      if (m->owner.compare_exchange_strong(owner, thread->tid))
        return {};
      if (owner == kUmutexContested) {
        if (m->owner.compare_exchange_strong(owner,
                                             thread->tid | kUmutexContested))
          return {};
        continue;
      }
    }

    if ((flags & kUmutexErrorCheck) != 0 &&
        (owner & ~kUmutexContested) == thread->tid)
      return ErrorCode::DEADLK;

    if (mode == umutex_lock_mode::try_)
      return ErrorCode::BUSY;

    if (error != ErrorCode{})
      return error;

    auto [chain, key, lock] = g_context.getUmtxChain1(thread, flags, m);
    auto node = chain.enqueue(key, thread);
    if (m->owner.compare_exchange_strong(owner, owner | kUmutexContested)) {
      orbis::scoped_unblock unblock;
      error = orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut));
      if (error == ErrorCode{} && node->second.thr == thread) {
        error = ErrorCode::TIMEDOUT;
      }
    }
    if (node->second.thr == thread)
      chain.erase(node);
  }
}
static ErrorCode do_lock_pi(Thread *thread, ptr<umutex> m, uint flags,
                            std::uint64_t ut, umutex_lock_mode mode) {
  // ORBIS_LOG_TODO(__FUNCTION__, m, flags, ut, mode);
  return do_lock_normal(thread, m, flags, ut, mode);
}
static ErrorCode do_lock_pp(Thread *thread, ptr<umutex> m, uint flags,
                            std::uint64_t ut, umutex_lock_mode mode) {
  // ORBIS_LOG_TODO(__FUNCTION__, m, flags, ut, mode);
  return do_lock_normal(thread, m, flags, ut, mode);
}
static ErrorCode do_unlock_normal(Thread *thread, ptr<umutex> m, uint flags) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, m, flags);

  int owner = m->owner.load(std::memory_order_acquire);
  if ((owner & ~kUmutexContested) != thread->tid)
    return ErrorCode::PERM;

  if ((owner & kUmtxContested) == 0) {
    if (m->owner.compare_exchange_strong(owner, kUmutexUnowned))
      return {};
  }

  auto [chain, key, lock] = g_context.getUmtxChain1(thread, flags, m);
  std::size_t count = chain.sleep_queue.count(key);
  bool ok = m->owner.compare_exchange_strong(
      owner, count <= 1 ? kUmutexUnowned : kUmutexContested);
  if (key.pid == 0) {
    // IPC workaround (TODO)
    chain.notify_all(key);
    if (!ok)
      return ErrorCode::INVAL;
    return {};
  }
  if (count)
    chain.notify_all(key);
  if (!ok)
    return ErrorCode::INVAL;
  return {};
}
static ErrorCode do_unlock_pi(Thread *thread, ptr<umutex> m, uint flags) {
  return do_unlock_normal(thread, m, flags);
}
static ErrorCode do_unlock_pp(Thread *thread, ptr<umutex> m, uint flags) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags);
  return do_unlock_normal(thread, m, flags);
}

} // namespace orbis

orbis::ErrorCode orbis::umtx_trylock_umutex(Thread *thread, ptr<umutex> m) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, m);
  uint flags;
  if (ErrorCode err = uread(flags, &m->flags); err != ErrorCode{})
    return err;
  switch (flags & (kUmutexPrioInherit | kUmutexPrioProtect)) {
  case 0:
    return do_lock_normal(thread, m, flags, 0, umutex_lock_mode::try_);
  case kUmutexPrioInherit:
    return do_lock_pi(thread, m, flags, 0, umutex_lock_mode::try_);
  case kUmutexPrioProtect:
    return do_lock_pp(thread, m, flags, 0, umutex_lock_mode::try_);
  }
  return ErrorCode::INVAL;
}

orbis::ErrorCode orbis::umtx_lock_umutex(Thread *thread, ptr<umutex> m,
                                         std::uint64_t ut) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, m, ut);
  uint flags;
  if (ErrorCode err = uread(flags, &m->flags); err != ErrorCode{})
    return err;
  switch (flags & (kUmutexPrioInherit | kUmutexPrioProtect)) {
  case 0:
    return do_lock_normal(thread, m, flags, ut, umutex_lock_mode::lock);
  case kUmutexPrioInherit:
    return do_lock_pi(thread, m, flags, ut, umutex_lock_mode::lock);
  case kUmutexPrioProtect:
    return do_lock_pp(thread, m, flags, ut, umutex_lock_mode::lock);
  }
  return ErrorCode::INVAL;
}

orbis::ErrorCode orbis::umtx_unlock_umutex(Thread *thread, ptr<umutex> m) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, m);
  uint flags;
  if (ErrorCode err = uread(flags, &m->flags); err != ErrorCode{})
    return err;
  switch (flags & (kUmutexPrioInherit | kUmutexPrioProtect)) {
  case 0:
    return do_unlock_normal(thread, m, flags);
  case kUmutexPrioInherit:
    return do_unlock_pi(thread, m, flags);
  case kUmutexPrioProtect:
    return do_unlock_pp(thread, m, flags);
  }
  return ErrorCode::INVAL;
}

orbis::ErrorCode orbis::umtx_set_ceiling(Thread *thread, ptr<umutex> m,
                                         std::uint32_t ceiling,
                                         ptr<uint32_t> oldCeiling) {
  ORBIS_LOG_TRACE(__FUNCTION__, m, ceiling, oldCeiling);
  std::abort();
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_cv_wait(Thread *thread, ptr<ucond> cv,
                                     ptr<umutex> m, std::uint64_t ut,
                                     ulong wflags) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, cv, m, ut, wflags);
  uint flags;
  if (ErrorCode err = uread(flags, &m->flags); err != ErrorCode{})
    return err;
  if ((wflags & ~(kCvWaitAbsTime | kCvWaitClockId))) {
    ORBIS_LOG_FATAL("umtx_cv_wait: UNKNOWN wflags", wflags);
    return ErrorCode::INVAL;
  }
  if ((wflags & kCvWaitClockId) != 0 && ut + 1 && cv->clockid != 0) {
    ORBIS_LOG_WARNING("umtx_cv_wait: CLOCK_ID", wflags, cv->clockid);
    // std::abort();
    return ErrorCode::NOSYS;
  }
  if ((wflags & kCvWaitAbsTime) != 0 && ut + 1) {
    ORBIS_LOG_WARNING("umtx_cv_wait: ABSTIME unimplemented", wflags);
    auto now = std::chrono::time_point_cast<std::chrono::microseconds>(
                   std::chrono::high_resolution_clock::now())
                   .time_since_epoch()
                   .count();

    if (now > ut) {
      ut = 0;
    } else {
      ut = ut - now;
    }

    std::abort();
    return ErrorCode::NOSYS;
  }

  auto [chain, key, lock] = g_context.getUmtxChain0(thread, cv->flags, cv);
  auto node = chain.enqueue(key, thread);

  if (!cv->has_waiters.load(std::memory_order::relaxed)) {
    cv->has_waiters.store(1, std::memory_order::relaxed);
  }

  ErrorCode result = umtx_unlock_umutex(thread, m);
  if (result == ErrorCode{}) {
    if (ut + 1 == 0) {
      while (true) {
        orbis::scoped_unblock unblock;
        result = orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut));
        if (result != ErrorCode{} || node->second.thr != thread) {
          break;
        }
      }
    } else {
      auto start = std::chrono::steady_clock::now();
      std::uint64_t udiff = 0;
      while (true) {
        orbis::scoped_unblock unblock;
        result =
            orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut - udiff));
        if (node->second.thr != thread) {
          break;
        }
        udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
        if (udiff >= ut) {
          result = ErrorCode::TIMEDOUT;
          break;
        }

        if (result != ErrorCode{}) {
          break;
        }
      }
    }
  }

  if (node->second.thr != thread) {
    result = {};
  } else {
    chain.erase(node);
    if (chain.sleep_queue.count(key) == 0)
      cv->has_waiters.store(0, std::memory_order::relaxed);
  }
  return result;
}

orbis::ErrorCode orbis::umtx_cv_signal(Thread *thread, ptr<ucond> cv) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, cv);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, cv->flags, cv);
  if (key.pid == 0) {
    // IPC workaround (TODO)
    chain.notify_all(key);
    cv->has_waiters = 0;
    return {};
  }
  std::size_t count = chain.sleep_queue.count(key);
  if (chain.notify_one(key) >= count)
    cv->has_waiters.store(0, std::memory_order::relaxed);
  return {};
}

orbis::ErrorCode orbis::umtx_cv_broadcast(Thread *thread, ptr<ucond> cv) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, cv);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, cv->flags, cv);
  chain.notify_all(key);
  cv->has_waiters.store(0, std::memory_order::relaxed);
  return {};
}

orbis::ErrorCode orbis::umtx_rw_rdlock(Thread *thread, ptr<urwlock> rwlock,
                                       slong fflag, ulong ut) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, rwlock, fflag, ut);
  auto flags = rwlock->flags;
  auto [chain, key, lock] = g_context.getUmtxChain1(thread, flags & 1, rwlock);

  auto wrflags = kUrwLockWriteOwner;
  if (!(fflag & kUrwLockPreferReader) && !(flags & kUrwLockPreferReader)) {
    wrflags |= kUrwLockWriteWaiters;
  }

  while (true) {
    auto state = rwlock->state.load(std::memory_order::relaxed);
    while ((state & wrflags) == 0) {
      if ((state & kUrwLockMaxReaders) == kUrwLockMaxReaders) {
        return ErrorCode::AGAIN;
      }

      if (rwlock->state.compare_exchange_strong(state, state + 1)) {
        return {};
      }
    }

    while ((state & wrflags) && !(state & kUrwLockReadWaiters)) {
      if (rwlock->state.compare_exchange_weak(state,
                                              state | kUrwLockReadWaiters)) {
        break;
      }
    }

    if (!(state & wrflags)) {
      continue;
    }

    ++rwlock->blocked_readers;

    ErrorCode result{};

    while (state & wrflags) {
      auto node = chain.enqueue(key, thread);

      if (ut + 1 == 0) {
        while (true) {
          orbis::scoped_unblock unblock;
          result = orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut));
          if (result != ErrorCode{} || node->second.thr != thread) {
            break;
          }
        }
      } else {
        auto start = std::chrono::steady_clock::now();
        std::uint64_t udiff = 0;
        while (true) {
          orbis::scoped_unblock unblock;
          result =
              orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut - udiff));
          if (node->second.thr != thread)
            break;
          udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();
          if (udiff >= ut) {
            result = ErrorCode::TIMEDOUT;
            break;
          }

          if (result != ErrorCode{}) {
            break;
          }
        }
      }

      if (node->second.thr != thread) {
        result = {};
      } else {
        chain.erase(node);
      }

      if (result != ErrorCode{}) {
        break;
      }

      state = rwlock->state.load(std::memory_order::relaxed);
    }

    if (--rwlock->blocked_readers == 0) {
      while (true) {
        if (!rwlock->state.compare_exchange_weak(
                state, state & ~kUrwLockReadWaiters)) {
          break;
        }
      }
    }
  }
  return {};
}

orbis::ErrorCode orbis::umtx_rw_wrlock(Thread *thread, ptr<urwlock> rwlock,
                                       ulong ut) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, rwlock, ut);

  auto flags = rwlock->flags;
  auto [chain, key, lock] = g_context.getUmtxChain1(thread, flags & 1, rwlock);

  uint32_t blocked_readers = 0;
  ErrorCode error = {};
  while (true) {
    auto state = rwlock->state.load(std::memory_order::relaxed);
    while (!(state & kUrwLockWriteOwner) && (state & kUrwLockMaxReaders) == 0) {
      if (!rwlock->state.compare_exchange_strong(state,
                                                 state | kUrwLockWriteOwner)) {
        return {};
      }
    }

    if (error != ErrorCode{}) {
      if (!(state & (kUrwLockWriteOwner | kUrwLockWriteWaiters)) &&
          blocked_readers != 0) {
        chain.notify_one(key);
      }

      break;
    }

    state = rwlock->state.load(std::memory_order::relaxed);

    while (
        ((state & kUrwLockWriteOwner) || (state & kUrwLockMaxReaders) != 0) &&
        (state & kUrwLockWriteWaiters) == 0) {
      if (!rwlock->state.compare_exchange_strong(
              state, state | kUrwLockWriteWaiters)) {
        break;
      }
    }

    if (!(state & kUrwLockWriteOwner) && (state & kUrwLockMaxReaders) == 0) {
      continue;
    }

    ++rwlock->blocked_writers;

    while ((state & kUrwLockWriteOwner) || (state & kUrwLockMaxReaders) != 0) {
      auto node = chain.enqueue(key, thread);

      if (ut + 1 == 0) {
        while (true) {
          orbis::scoped_unblock unblock;
          error = orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut));
          if (error != ErrorCode{} || node->second.thr != thread) {
            break;
          }
        }
      } else {
        auto start = std::chrono::steady_clock::now();
        std::uint64_t udiff = 0;
        while (true) {
          orbis::scoped_unblock unblock;
          error =
              orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut - udiff));
          if (node->second.thr != thread)
            break;
          udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();
          if (udiff >= ut) {
            error = ErrorCode::TIMEDOUT;
            break;
          }
          if (error != ErrorCode{}) {
            break;
          }
        }
      }

      if (node->second.thr != thread) {
        error = {};
      } else {
        chain.erase(node);
      }

      if (error != ErrorCode{}) {
        break;
      }

      state = rwlock->state.load(std::memory_order::relaxed);
    }

    if (--rwlock->blocked_writers == 0) {
      state = rwlock->state.load(std::memory_order::relaxed);

      while (true) {
        if (rwlock->state.compare_exchange_weak(
                state, state & ~kUrwLockWriteWaiters)) {
          break;
        }
      }
      blocked_readers = rwlock->blocked_readers;
    } else {
      blocked_readers = 0;
    }
  }

  return error;
}

orbis::ErrorCode orbis::umtx_rw_unlock(Thread *thread, ptr<urwlock> rwlock) {
  auto flags = rwlock->flags;
  auto [chain, key, lock] = g_context.getUmtxChain1(thread, flags & 1, rwlock);

  auto state = rwlock->state.load(std::memory_order::relaxed);
  if (state & kUrwLockWriteOwner) {
    while (true) {
      if (rwlock->state.compare_exchange_weak(state,
                                              state & ~kUrwLockWriteOwner)) {
        break;
      }

      if (!(state & kUrwLockWriteOwner)) {
        return ErrorCode::PERM;
      }
    }
  } else if ((state & kUrwLockMaxReaders) != 0) {
    while (true) {
      if (rwlock->state.compare_exchange_weak(state, state - 1)) {
        break;
      }

      if ((state & kUrwLockMaxReaders) == 0) {
        return ErrorCode::PERM;
      }
    }
  } else {
    return ErrorCode::PERM;
  }

  unsigned count = 0;

  if (!(flags & kUrwLockPreferReader)) {
    if (state & kUrwLockWriteWaiters) {
      count = 1;
    } else if (state & kUrwLockReadWaiters) {
      count = UINT_MAX;
    }
  } else {
    if (state & kUrwLockReadWaiters) {
      count = UINT_MAX;
    } else if (state & kUrwLockWriteWaiters) {
      count = 1;
    }
  }

  chain.notify_n(key, count);
  return {};
}

orbis::ErrorCode orbis::umtx_wake_private(Thread *thread, ptr<void> addr,
                                          sint n_wake) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, addr, n_wake);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, false, addr);
  chain.notify_n(key, n_wake);
  return {};
}

orbis::ErrorCode orbis::umtx_wait_umutex(Thread *thread, ptr<umutex> m,
                                         std::uint64_t ut) {
  ORBIS_LOG_TRACE(__FUNCTION__, m, ut);
  uint flags;
  if (ErrorCode err = uread(flags, &m->flags); err != ErrorCode{})
    return err;
  switch (flags & (kUmutexPrioInherit | kUmutexPrioProtect)) {
  case 0:
    return do_lock_normal(thread, m, flags, ut, umutex_lock_mode::wait);
  case kUmutexPrioInherit:
    return do_lock_pi(thread, m, flags, ut, umutex_lock_mode::wait);
  case kUmutexPrioProtect:
    return do_lock_pp(thread, m, flags, ut, umutex_lock_mode::wait);
  }
  return ErrorCode::INVAL;
}

orbis::ErrorCode orbis::umtx_wake_umutex(Thread *thread, ptr<umutex> m,
                                         sint wakeFlags) {
  ORBIS_LOG_TRACE(__FUNCTION__, m);
  int owner = m->owner.load(std::memory_order::acquire);
  if ((owner & ~kUmutexContested) != 0)
    return {};

  uint flags;
  if (ErrorCode err = uread(flags, &m->flags); err != ErrorCode{})
    return err;

  auto [chain, key, lock] = g_context.getUmtxChain1(thread, flags, m);
  std::size_t count = chain.sleep_queue.count(key);
  if (count <= 1) {
    owner = kUmutexContested;
    m->owner.compare_exchange_strong(owner, kUmutexUnowned);
  }
  if (count != 0 && (owner & ~kUmutexContested) == 0) {
    if ((wakeFlags & 0x400) || (flags & 1)) {
      chain.notify_all(key);
    } else {
      chain.notify_one(key);
    }
  }
  return {};
}

orbis::ErrorCode orbis::umtx_sem_wait(Thread *thread, ptr<usem> sem,
                                      std::uint64_t ut) {
  ORBIS_LOG_TRACE(__FUNCTION__, sem, ut);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, sem->flags, sem);
  auto node = chain.enqueue(key, thread);

  std::uint32_t has_waiters = sem->has_waiters;
  if (!has_waiters)
    sem->has_waiters.compare_exchange_strong(has_waiters, 1);

  ErrorCode result = {};
  if (!sem->count) {
    if (ut + 1 == 0) {
      while (true) {
        orbis::scoped_unblock unblock;
        result = orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut));
        if (result != ErrorCode{} || node->second.thr != thread)
          break;
      }
    } else {
      auto start = std::chrono::steady_clock::now();
      std::uint64_t udiff = 0;
      while (true) {
        orbis::scoped_unblock unblock;
        result =
            orbis::toErrorCode(node->second.cv.wait(chain.mtx, ut - udiff));
        if (node->second.thr != thread)
          break;
        udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
        if (udiff >= ut) {
          result = ErrorCode::TIMEDOUT;
          break;
        }
        if (result != ErrorCode{}) {
          break;
        }
      }
    }
  }

  if (node->second.thr != thread) {
    result = {};
  } else {
    chain.erase(node);
  }
  return result;
}

orbis::ErrorCode orbis::umtx_sem_wake(Thread *thread, ptr<usem> sem) {
  ORBIS_LOG_TRACE(__FUNCTION__, sem);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, sem->flags, sem);
  if (key.pid == 0) {
    // IPC workaround (TODO)
    chain.notify_all(key);
    sem->has_waiters = 0;
    return {};
  }
  std::size_t count = chain.sleep_queue.count(key);
  if (chain.notify_one(key) >= count)
    sem->has_waiters.store(0, std::memory_order::relaxed);
  return {};
}

orbis::ErrorCode orbis::umtx_nwake_private(Thread *thread, ptr<void *> uaddrs,
                                           std::int64_t count) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, uaddrs, count);
  while (count-- > 0) {
    void *uaddr;
    auto error = uread(uaddr, uaddrs++);
    if (error != ErrorCode{})
      return error;
    umtx_wake_private(thread, uaddr, 1);
  }
  return {};
}

orbis::ErrorCode orbis::umtx_wake2_umutex(Thread *thread, ptr<void> obj,
                                          std::int64_t val, ptr<void> uaddr1,
                                          ptr<void> uaddr2) {
  ORBIS_LOG_TODO(__FUNCTION__, obj, val, uaddr1, uaddr2);
  std::abort();
  return ErrorCode::NOSYS;
}
