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

void UmtxChain::notify_one(const UmtxKey &key) {
  auto it = sleep_queue.find(key);
  if (it == sleep_queue.end())
    return;
  it->second.thr = nullptr;
  it->second.cv.notify_one(mtx);
  this->erase(&*it);
}
} // namespace orbis

orbis::ErrorCode orbis::umtx_lock_umtx(Thread *thread, ptr<umtx> umtx, ulong id,
                                       std::uint64_t ut) {
  ORBIS_LOG_TODO(__FUNCTION__, umtx, id, ut);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_unlock_umtx(Thread *thread, ptr<umtx> umtx,
                                         ulong id) {
  ORBIS_LOG_TODO(__FUNCTION__, umtx, id);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait(Thread *thread, ptr<void> addr, ulong id,
                                  std::uint64_t ut) {
  ORBIS_LOG_NOTICE(__FUNCTION__, addr, id, ut);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread->tproc->pid, addr);
  auto node = chain.enqueue(key, thread);
  ErrorCode result = {};
  // TODO: this is inaccurate with timeout as FreeBsd 9.1 only checks id once
  if (reinterpret_cast<ptr<std::atomic<ulong>>>(addr)->load() == id) {
    node->second.cv.wait(chain.mtx, ut);
    if (node->second.thr == thread)
      result = ErrorCode::TIMEDOUT;
  }
  if (node->second.thr == thread)
    chain.erase(node);
  return result;
}

orbis::ErrorCode orbis::umtx_wake(Thread *thread, ptr<void> addr, sint n_wake) {
  ORBIS_LOG_NOTICE(__FUNCTION__, addr, n_wake);
  auto [chain, key, lock] = g_context.getUmtxChain0(thread->tproc->pid, addr);
  std::size_t count = chain.sleep_queue.count(key);
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
  ORBIS_LOG_NOTICE(__FUNCTION__, m, flags, ut, mode);

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

    auto [chain, key, lock] = g_context.getUmtxChain1(thread->tproc->pid, m);
    auto node = chain.enqueue(key, thread);
    if (m->owner.compare_exchange_strong(owner, owner | kUmutexContested)) {
      node->second.cv.wait(chain.mtx, ut);
      if (node->second.thr == thread)
        error = ErrorCode::TIMEDOUT;
    }
    if (node->second.thr == thread)
      chain.erase(node);
  }

  return {};
}
static ErrorCode do_lock_pi(Thread *thread, ptr<umutex> m, uint flags,
                            std::uint64_t ut, umutex_lock_mode mode) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags, ut, mode);
  return ErrorCode::NOSYS;
}
static ErrorCode do_lock_pp(Thread *thread, ptr<umutex> m, uint flags,
                            std::uint64_t ut, umutex_lock_mode mode) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags, ut, mode);
  return ErrorCode::NOSYS;
}
static ErrorCode do_unlock_normal(Thread *thread, ptr<umutex> m, uint flags) {
  ORBIS_LOG_NOTICE(__FUNCTION__, m, flags);

  int owner = m->owner.load(std::memory_order_acquire);
  if ((owner & ~kUmutexContested) != thread->tid)
    return ErrorCode::PERM;

  if ((owner & kUmtxContested) == 0) {
    if (m->owner.compare_exchange_strong(owner, kUmutexUnowned))
      return {};
  }

  auto [chain, key, lock] = g_context.getUmtxChain1(thread->tproc->pid, m);
  std::size_t count = chain.sleep_queue.count(key);
  bool ok = m->owner.compare_exchange_strong(
      owner, count <= 1 ? kUmutexUnowned : kUmutexContested);
  if (count)
    chain.notify_one(key);
  if (!ok)
    return ErrorCode::INVAL;
  return {};
}
static ErrorCode do_unlock_pi(Thread *thread, ptr<umutex> m, uint flags) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags);
  return ErrorCode::NOSYS;
}
static ErrorCode do_unlock_pp(Thread *thread, ptr<umutex> m, uint flags) {
  ORBIS_LOG_TODO(__FUNCTION__, m, flags);
  return ErrorCode::NOSYS;
}

} // namespace orbis

orbis::ErrorCode orbis::umtx_trylock_umutex(Thread *thread, ptr<umutex> m) {
  ORBIS_LOG_TRACE(__FUNCTION__, m);
  uint flags = uread(&m->flags);
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
  ORBIS_LOG_TRACE(__FUNCTION__, m, ut);
  uint flags = uread(&m->flags);
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
  ORBIS_LOG_TRACE(__FUNCTION__, m);
  uint flags = uread(&m->flags);
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
  ORBIS_LOG_TODO(__FUNCTION__, cv, m, ut, wflags);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_cv_signal(Thread *thread, ptr<ucond> cv) {
  ORBIS_LOG_TODO(__FUNCTION__, cv);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_cv_broadcast(Thread *thread, ptr<ucond> cv) {
  ORBIS_LOG_TODO(__FUNCTION__, cv);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait_uint(Thread *thread, ptr<void> addr, ulong id,
                                       std::uint64_t ut) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, id, ut);
  return ErrorCode::NOSYS;
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

orbis::ErrorCode orbis::umtx_wait_uint_private(Thread *thread, ptr<void> addr,
                                               ulong id, std::uint64_t ut) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, id, ut);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wake_private(Thread *thread, ptr<void> uaddr,
                                          sint n_wake) {
  ORBIS_LOG_TODO(__FUNCTION__, uaddr, n_wake);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wait_umutex(Thread *thread, ptr<umutex> m,
                                         std::uint64_t ut) {
  ORBIS_LOG_TRACE(__FUNCTION__, m, ut);
  uint flags = uread(&m->flags);
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

  [[maybe_unused]] uint flags = uread(&m->flags);

  auto [chain, key, lock] = g_context.getUmtxChain1(thread->tproc->pid, m);
  std::size_t count = chain.sleep_queue.count(key);
  if (count <= 1) {
    owner = kUmutexContested;
    m->owner.compare_exchange_strong(owner, kUmutexUnowned);
  }
  if (count != 0 && (owner & ~kUmutexContested) == 0)
    chain.notify_one(key);
  return {};
}

orbis::ErrorCode orbis::umtx_sem_wait(Thread *thread, ptr<void> obj,
                                      std::int64_t val, ptr<void> uaddr1,
                                      ptr<void> uaddr2) {
  ORBIS_LOG_TODO(__FUNCTION__, obj, val, uaddr1, uaddr2);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_sem_wake(Thread *thread, ptr<void> obj,
                                      std::int64_t val, ptr<void> uaddr1,
                                      ptr<void> uaddr2) {
  ORBIS_LOG_TODO(__FUNCTION__, obj, val, uaddr1, uaddr2);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_nwake_private(Thread *thread, ptr<void> uaddrs,
                                           std::int64_t count) {
  ORBIS_LOG_TODO(__FUNCTION__, uaddrs, count);
  return ErrorCode::NOSYS;
}

orbis::ErrorCode orbis::umtx_wake2_umutex(Thread *thread, ptr<void> obj,
                                          std::int64_t val, ptr<void> uaddr1,
                                          ptr<void> uaddr2) {
  ORBIS_LOG_TODO(__FUNCTION__, obj, val, uaddr1, uaddr2);
  return ErrorCode::NOSYS;
}
