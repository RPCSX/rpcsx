#include "KernelAllocator.hpp"
#include "sys/sysproto.hpp"

#include "thread/Process.hpp"
#include "utils/Logs.hpp"
#include "utils/SharedMutex.hpp"
#include <chrono>
#include <list>
#include <span>
#include <thread>

namespace orbis {
struct KEvent {
  uintptr_t ident;
  sshort filter;
  ushort flags;
  uint fflags;
  intptr_t data;
  ptr<void> udata;
};

struct KNote {
  KEvent event;
  bool enabled;
};

struct KQueue : orbis::File {
  std::list<KNote, kallocator<KNote>> notes;
};

static constexpr auto kEvFiltRead = -1;
static constexpr auto kEvFiltWrite = -2;
static constexpr auto kEvFiltAio = -3;
static constexpr auto kEvFiltVnode = -4;
static constexpr auto kEvFiltProc = -5;
static constexpr auto kEvFiltSignal = -6;
static constexpr auto kEvFiltTimer = -7;
static constexpr auto kEvFiltFs = -9;
static constexpr auto kEvFiltLio = -10;
static constexpr auto kEvFiltUser = -11;
static constexpr auto kEvFiltPolling = -12;
static constexpr auto kEvFiltDisplay = -13;
static constexpr auto kEvFiltGraphicsCore = -14;
static constexpr auto kEvFiltHrTimer = -15;
static constexpr auto kEvFiltUvdTrap = -16;
static constexpr auto kEvFiltVceTrap = -17;
static constexpr auto kEvFiltSdmaTrap = -18;
static constexpr auto kEvFiltRegEv = -19;
static constexpr auto kEvFiltGpuException = -20;
static constexpr auto kEvFiltGpuSystemException = -21;
static constexpr auto kEvFiltGpuDbgGcEv = -22;
static constexpr auto kEvFiltSysCount = 22;

// actions
static constexpr auto kEvAdd = 0x0001;
static constexpr auto kEvDelete = 0x0002;
static constexpr auto kEvEnable = 0x0004;
static constexpr auto kEvDisable = 0x0008;

// flags
static constexpr auto kEvOneshot = 0x0010;
static constexpr auto kEvClear = 0x0020;
static constexpr auto kEvReceipt = 0x0040;
static constexpr auto kEvDispatch = 0x0080;
static constexpr auto kEvSysFlags = 0xf000;
static constexpr auto kEvFlag1 = 0x2000;

static constexpr auto kEvEof = 0x8000;
static constexpr auto kEvError = 0x4000;

// kEvFiltUser
static constexpr auto kNoteFFNop = 0x00000000;
static constexpr auto kNoteFFAnd = 0x40000000;
static constexpr auto kNoteFFOr = 0x80000000;
static constexpr auto kNoteFFCopy = 0xc0000000;
static constexpr auto kNoteFFCtrlMask = 0xc0000000;
static constexpr auto kNoteFFlagsMask = 0x00ffffff;
static constexpr auto kNoteTrigger = 0x01000000;
} // namespace orbis

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
static SysResult keventChange(KQueue *kq, KEvent &change) {
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

  if (change.flags & kEvAdd) {
    if (nodeIt == kq->notes.end()) {
      KNote note {
        .event = change,
        .enabled = true,
      };

      note.event.flags &= ~(kEvAdd | kEvDelete | kEvDisable | kEvEnable);
      kq->notes.push_front(note);
      nodeIt = kq->notes.begin();
    }
  }

  if (nodeIt == kq->notes.end()) {
    if (change.flags & kEvDelete) {
      return{};
    }

    return orbis::ErrorCode::NOENT;
  }

  if (change.flags & kEvDisable) {
    nodeIt->enabled = false;
  }
  if (change.flags & kEvEnable) {
    nodeIt->enabled = true;
  }

  if (change.filter == kEvFiltUser) {
    auto fflags = 0;
    switch (change.fflags & kNoteFFCtrlMask) {
    case kNoteFFAnd: fflags = nodeIt->event.fflags & change.fflags; break;
    case kNoteFFOr: fflags = nodeIt->event.fflags | change.fflags; break;
    case kNoteFFCopy: fflags = change.fflags; break;
    }

    nodeIt->event.fflags = (nodeIt->event.fflags & ~kNoteFFlagsMask) | (fflags & kNoteFFlagsMask);

    if (change.fflags & kNoteTrigger) {
      nodeIt->event.fflags |= kNoteTrigger;
    }

    if (change.flags & kEvClear) {
      nodeIt->event.fflags &= ~kNoteTrigger;
    }
  }

  return{};
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
  Ref<File> kqf = thread->tproc->fileDescriptors.get(fd);
  if (kqf == nullptr) {
    return orbis::ErrorCode::BADF;
  }

  std::lock_guard lock(kqf->mtx);

  auto kq = dynamic_cast<KQueue *>(kqf.get());

  if (kq == nullptr) {
    return orbis::ErrorCode::BADF;
  }

  if (nchanges != 0) {
    for (auto change : std::span(changelist, nchanges)) {
      ORBIS_LOG_TODO(__FUNCTION__, change.ident, change.filter, change.flags,
                     change.fflags, change.data, change.udata);

      if (auto result = keventChange(kq, change); result.value() != 0) {
        return result;
      }
    }
  }

  if (nevents == 0) {
    return{};
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

  while (result.empty()) {
    for (auto it = kq->notes.begin(); it != kq->notes.end();) {
      if (result.size() >= nevents) {
        break;
      }

      auto &note = *it;

      if (!note.enabled) {
        ++it;
        continue;
      }

      if (note.event.filter == kEvFiltUser) {
        if ((note.event.fflags & kNoteTrigger) == 0) {
          ++it;
          continue;
        }

        auto event = note.event;
        event.fflags &= kNoteFFlagsMask;
        result.push_back(event);
      } else if (note.event.filter == kEvFiltDisplay ||
          note.event.filter == kEvFiltGraphicsCore) {
        result.push_back(note.event);
      } else if (note.event.filter == kEvFiltProc) {
        // TODO
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        note.event.data = 0;
        result.push_back(note.event);
      } else {
        ++it;
        continue;
      }

      if (note.event.flags & kEvOneshot) {
        it = kq->notes.erase(it);
      } else {
        ++it;
      }
    }

    if (result.empty() && timeoutPoint != clock::time_point::max()) {
      if (clock::now() >= timeoutPoint) {
        break;
      }
    }
  }

  std::memcpy(eventlist, result.data(), result.size() * sizeof(KEvent));
  thread->retval[0] = result.size();
  return {};
}
