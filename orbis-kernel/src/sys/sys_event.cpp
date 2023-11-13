#include "KernelAllocator.hpp"
#include "KernelContext.hpp"
#include "sys/sysproto.hpp"

#include "thread/Process.hpp"
#include "utils/Logs.hpp"
#include <chrono>
#include <list>
#include <span>

orbis::SysResult orbis::sys_kqueue(Thread *thread) {
  auto queue = knew<KQueue>();
  if (queue == nullptr) {
    return ErrorCode::NOMEM;
  }

  auto fd = thread->tproc->fileDescriptors.insert(queue);
  ORBIS_LOG_TODO(__FUNCTION__, fd);
  thread->retval[0] = fd;
  return {};
}

orbis::SysResult orbis::sys_kqueueex(Thread *thread, ptr<char> name,
                                     sint flags) {
  auto queue = knew<KQueue>();
  if (queue == nullptr) {
    return ErrorCode::NOMEM;
  }

  auto fd = thread->tproc->fileDescriptors.insert(queue);
  ORBIS_LOG_TODO(__FUNCTION__, name, flags, fd);
  thread->retval[0] = fd;
  return {};
}

namespace orbis {
static SysResult keventChange(KQueue *kq, KEvent &change, Thread *thread) {
  auto nodeIt = kq->notes.end();
  for (auto it = kq->notes.begin(); it != kq->notes.end(); ++it) {
    if (it->event.ident == change.ident && it->event.filter == change.filter) {
      nodeIt = it;
      break;
    }
  }

  if (change.flags & kEvDelete) {
    if (nodeIt == kq->notes.end()) {
      return orbis::ErrorCode::NOENT;
    }

    kq->notes.erase(nodeIt);
    nodeIt = kq->notes.end();
  }

  std::unique_lock<shared_mutex> noteLock;
  if (change.flags & kEvAdd) {
    if (nodeIt == kq->notes.end()) {
      auto &note = kq->notes.emplace_front();
      note.event.flags &= ~(kEvAdd | kEvDelete | kEvDisable | kEvEnable);
      note.queue = kq;
      note.event = change;
      note.enabled = true;
      nodeIt = kq->notes.begin();

      if (change.filter == kEvFiltProc) {
        auto process = g_context.findProcessById(change.ident);
        if (process == nullptr) {
          return ErrorCode::SRCH;
        }

        noteLock = std::unique_lock(nodeIt->mutex);

        std::unique_lock lock(process->event.mutex);
        process->event.notes.insert(&*nodeIt);
        nodeIt->linked = process;
      } else if (change.filter == kEvFiltRead ||
                 change.filter == kEvFiltWrite) {
        auto fd = thread->tproc->fileDescriptors.get(change.ident);

        if (fd == nullptr) {
          return ErrorCode::BADF;
        }

        std::unique_lock lock(fd->event.mutex);

        if (change.filter == kEvFiltWrite) {
          nodeIt->triggered = true;
          kq->cv.notify_all(kq->mtx);
        }

        fd->event.notes.insert(&*nodeIt);
      }
    }
  }

  if (nodeIt == kq->notes.end()) {
    if (change.flags & kEvDelete) {
      return {};
    }

    return orbis::ErrorCode::NOENT;
  }

  if (!noteLock.owns_lock()) {
    noteLock = std::unique_lock(nodeIt->mutex);
  }

  if (change.flags & kEvDisable) {
    nodeIt->enabled = false;
  }
  if (change.flags & kEvEnable) {
    nodeIt->enabled = true;
  }
  if (change.flags & kEvClear) {
    nodeIt->triggered = false;
  }

  if (change.filter == kEvFiltUser) {
    auto fflags = 0;
    switch (change.fflags & kNoteFFCtrlMask) {
    case kNoteFFAnd:
      fflags = nodeIt->event.fflags & change.fflags;
      break;
    case kNoteFFOr:
      fflags = nodeIt->event.fflags | change.fflags;
      break;
    case kNoteFFCopy:
      fflags = change.fflags;
      break;
    }

    nodeIt->event.fflags =
        (nodeIt->event.fflags & ~kNoteFFlagsMask) | (fflags & kNoteFFlagsMask);

    if (change.fflags & kNoteTrigger) {
      nodeIt->triggered = true;
      kq->cv.notify_all(kq->mtx);
    }
  } else if (change.filter == kEvFiltGraphicsCore ||
             change.filter == kEvFiltDisplay || change.filter == kEvFiltRegEv) {
    nodeIt->triggered = true;
    kq->cv.notify_all(kq->mtx);
  }

  return {};
}

static orbis::ErrorCode ureadTimespec(orbis::timespec &ts,
                                      orbis::ptr<const orbis::timespec> addr) {
  orbis::ErrorCode error = uread(ts, addr);
  if (error != orbis::ErrorCode{})
    return error;
  if (ts.sec < 0 || ts.nsec < 0 || ts.nsec > 1000000000) {
    return orbis::ErrorCode::INVAL;
  }

  return {};
}
} // namespace orbis

orbis::SysResult orbis::sys_kevent(Thread *thread, sint fd,
                                   ptr<KEvent> changelist, sint nchanges,
                                   ptr<KEvent> eventlist, sint nevents,
                                   ptr<const timespec> timeout) {
  // ORBIS_LOG_TODO(__FUNCTION__, fd, changelist, nchanges, eventlist, nevents,
  //                timeout);
  auto kq = thread->tproc->fileDescriptors.get(fd).cast<KQueue>();
  if (kq == nullptr) {
    return orbis::ErrorCode::BADF;
  }

  {
    std::lock_guard lock(kq->mtx);

    if (nchanges != 0) {
      for (auto &changePtr : std::span(changelist, nchanges)) {
        KEvent change;
        ORBIS_RET_ON_ERROR(uread(change, &changePtr));
        ORBIS_LOG_TODO(__FUNCTION__, change.ident, change.filter, change.flags,
                       change.fflags, change.data, change.udata);

        if (auto result = keventChange(kq.get(), change, thread);
            result.value() != 0) {
          return result;
        }
      }
    }
  }

  if (nevents == 0) {
    return {};
  }

  using clock = std::chrono::high_resolution_clock;
  clock::time_point timeoutPoint = clock::time_point::max();
  if (timeout != nullptr) {
    timespec _timeout;
    auto result = ureadTimespec(_timeout, timeout);
    if (result != ErrorCode{}) {
      return result;
    }

    __uint128_t nsec = _timeout.sec;
    nsec *= 1000'000'000;
    nsec += _timeout.nsec;

    if (nsec < INT64_MAX) {
      auto now = clock::now();
      auto nowValue = now.time_since_epoch().count();

      if (nowValue < nowValue + nsec) {
        timeoutPoint = now + std::chrono::nanoseconds(nsec);
      }
    }
  }

  std::vector<KEvent> result;
  result.reserve(nevents);

  while (true) {
    std::lock_guard lock(kq->mtx);
    for (auto it = kq->notes.begin(); it != kq->notes.end();) {
      if (result.size() >= nevents) {
        break;
      }

      auto &note = *it;
      std::lock_guard lock(note.mutex);

      if (note.enabled && note.triggered) {
        result.push_back(note.event);

        if (note.event.flags & kEvDispatch) {
          note.enabled = false;
        }

        if (note.event.flags & kEvOneshot) {
          it = kq->notes.erase(it);
          continue;
        }
      }

      ++it;
    }

    if (!result.empty()) {
      break;
    }

    if (timeoutPoint != clock::time_point::max()) {
      auto now = clock::now();

      if (now >= timeoutPoint) {
        break;
      }

      auto waitTimeout = std::chrono::duration_cast<std::chrono::microseconds>(
          timeoutPoint - now);
      kq->cv.wait(kq->mtx, waitTimeout.count());
    } else {
      kq->cv.wait(kq->mtx);
    }
  }

  // ORBIS_LOG_TODO(__FUNCTION__, "kevent wakeup", fd);

  ORBIS_RET_ON_ERROR(
      uwriteRaw(eventlist, result.data(), result.size() * sizeof(KEvent)));
  thread->retval[0] = result.size();
  return {};
}
