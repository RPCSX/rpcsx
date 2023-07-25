#include "umtx.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/thread.hpp"
#include "orbis/utils/AtomicOp.hpp"
#include "orbis/utils/Logs.hpp"
#include "time.hpp"

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
      auto node = sleep_queue.extract(it);
      node.key() = {};
      spare_queue.insert(spare_queue.begin(), std::move(node));
      return;
    }
  }
}

uint UmtxChain::notify_one(const UmtxKey &key) {
  auto it = sleep_queue.find(key);
  if (it == sleep_queue.end())
    return 0;
  it->second.thr = nullptr;
  it->second.cv.notify_one(mtx);
  this->erase(&*it);
  return 1;
}

uint UmtxChain::notify_all(const UmtxKey &key) {
  uint n = 0;
  while (notify_one(key))
    n++;
  return n;
}
} // namespace orbis

orbis::ErrorCode orbis::umtx_lock_umtx(Thread *thread, ptr<umtx> umtx, ulong id,
                                       std::uint64_t ut) {
  ORBIS_LOG_TODO(__FUNCTION__, thread->tid, umtx, id, ut);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_unlock_umtx(Thread *thread, ptr<umtx> umtx,
                                         ulong id) {
  ORBIS_LOG_TODO(__FUNCTION__, thread->tid, umtx, id);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait(Thread *thread, ptr<void> addr, ulong id,
                                  std::uint64_t ut, bool is32, bool ipc) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, addr, id, ut, is32);
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
        node->second.cv.wait(chain.mtx);
        if (node->second.thr != thread)
          break;
      }
    } else {
      auto start = std::chrono::steady_clock::now();
      std::uint64_t udiff = 0;
      while (true) {
        node->second.cv.wait(chain.mtx, ut - udiff);
        if (node->second.thr != thread)
          break;
        udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
        if (udiff >= ut) {
          result = ErrorCode::TIMEDOUT;
          break;
        }
      }
    }
  }
  if (node->second.thr == thread)
    chain.erase(node);
  return result;
}

orbis::ErrorCode orbis::umtx_wake(Thread *thread, ptr<void> addr, sint n_wake) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, addr, n_wake);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, true, addr);
  std::size_t count = chain.sleep_queue.count(key);
  if (key.pid == 0) {
    // IPC workaround (TODO)
    chain.notify_all(key);
    return {};
  }
  // TODO: check this
  while (count--) {
    chain.notify_one(key);
    if (n_wake-- <= 1)
      break;
  }
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
      node->second.cv.wait(chain.mtx, ut);
      if (node->second.thr == thread) {
        error = ErrorCode::TIMEDOUT;
      }
    }
    if (node->second.thr == thread)
      chain.erase(node);
  }

  return {};
}
static ErrorCode do_lock_pi(Thread *thread, ptr<umutex> m, uint flags,
                            std::uint64_t ut, umutex_lock_mode mode) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags, ut, mode);
  return do_lock_normal(thread, m, flags, ut, mode);
}
static ErrorCode do_lock_pp(Thread *thread, ptr<umutex> m, uint flags,
                            std::uint64_t ut, umutex_lock_mode mode) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags, ut, mode);
  return ErrorCode::NOSYS;
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
    chain.notify_one(key);
  if (!ok)
    return ErrorCode::INVAL;
  return {};
}
static ErrorCode do_unlock_pi(Thread *thread, ptr<umutex> m, uint flags) {
  return do_unlock_normal(thread, m, flags);
}
static ErrorCode do_unlock_pp(Thread *thread, ptr<umutex> m, uint flags) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags);
  return ErrorCode::NOSYS;
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
  ORBIS_LOG_TODO(__FUNCTION__, m, ceiling, oldCeiling);
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
  if ((wflags & kCvWaitClockId) != 0 && ut + 1) {
    ORBIS_LOG_FATAL("umtx_cv_wait: CLOCK_ID unimplemented", wflags);
    return ErrorCode::NOSYS;
  }
  if ((wflags & kCvWaitAbsTime) != 0 && ut + 1) {
    ORBIS_LOG_FATAL("umtx_cv_wait: ABSTIME unimplemented", wflags);
    return ErrorCode::NOSYS;
  }

  auto [chain, key, lock] = g_context.getUmtxChain0(thread, cv->flags, cv);
  auto node = chain.enqueue(key, thread);

  if (!cv->has_waiters.load(std::memory_order::relaxed))
    cv->has_waiters.store(1, std::memory_order::relaxed);

  ErrorCode result = umtx_unlock_umutex(thread, m);
  if (result == ErrorCode{}) {
    if (ut + 1 == 0) {
      while (true) {
        node->second.cv.wait(chain.mtx, ut);
        if (node->second.thr != thread)
          break;
      }
    } else {
      auto start = std::chrono::steady_clock::now();
      std::uint64_t udiff = 0;
      while (true) {
        node->second.cv.wait(chain.mtx, ut - udiff);
        if (node->second.thr != thread)
          break;
        udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
        if (udiff >= ut) {
          result = ErrorCode::TIMEDOUT;
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

orbis::ErrorCode orbis::umtx_rw_rdlock(Thread *thread, ptr<void> obj,
                                       std::int64_t val, ptr<void> uaddr1,
                                       ptr<void> uaddr2) {
  ORBIS_LOG_TODO(__FUNCTION__, obj, val, uaddr1, uaddr2);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_rw_wrlock(Thread *thread, ptr<void> obj,
                                       std::int64_t val, ptr<void> uaddr1,
                                       ptr<void> uaddr2) {
  ORBIS_LOG_TODO(__FUNCTION__, obj, val, uaddr1, uaddr2);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_rw_unlock(Thread *thread, ptr<void> obj,
                                       std::int64_t val, ptr<void> uaddr1,
                                       ptr<void> uaddr2) {
  ORBIS_LOG_TODO(__FUNCTION__, obj, val, uaddr1, uaddr2);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wake_private(Thread *thread, ptr<void> addr,
                                          sint n_wake) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, addr, n_wake);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, false, addr);
  std::size_t count = chain.sleep_queue.count(key);
  // TODO: check this
  while (count--) {
    chain.notify_one(key);
    if (n_wake-- <= 1)
      break;
  }
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

orbis::ErrorCode orbis::umtx_wake_umutex(Thread *thread, ptr<umutex> m) {
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
  if (count != 0 && (owner & ~kUmutexContested) == 0)
    chain.notify_one(key);
  return {};
}

orbis::ErrorCode orbis::umtx_sem_wait(Thread *thread, ptr<usem> sem,
                                      std::uint64_t ut) {
  ORBIS_LOG_WARNING(__FUNCTION__, sem, ut);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread, sem->flags, sem);
  auto node = chain.enqueue(key, thread);

  std::uint32_t has_waiters = sem->has_waiters;
  if (!has_waiters)
    sem->has_waiters.compare_exchange_strong(has_waiters, 1);

  ErrorCode result = {};
  if (!sem->count) {
    if (ut + 1 == 0) {
      while (true) {
        node->second.cv.wait(chain.mtx, ut);
        if (node->second.thr != thread)
          break;
      }
    } else {
      auto start = std::chrono::steady_clock::now();
      std::uint64_t udiff = 0;
      while (true) {
        node->second.cv.wait(chain.mtx, ut - udiff);
        if (node->second.thr != thread)
          break;
        udiff = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
        if (udiff >= ut) {
          result = ErrorCode::TIMEDOUT;
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
  return ErrorCode::NOSYS;
}
