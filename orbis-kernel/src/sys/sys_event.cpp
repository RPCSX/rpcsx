#include "KernelAllocator.hpp"
#include "sys/sysproto.hpp"

#include "thread/Process.hpp"
#include "utils/Logs.hpp"
#include "utils/SharedMutex.hpp"
#include <list>
#include <span>

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

orbis::SysResult orbis::sys_kqueueex(Thread *thread, ptr<char> name, sint flags) {
  auto queue = knew<KQueue>();
  if (queue == nullptr) {
    return ErrorCode::NOMEM;
  }

  auto fd = thread->tproc->fileDescriptors.insert(queue);
  ORBIS_LOG_TODO(__FUNCTION__, name, flags, fd);
  thread->retval[0] = fd;
  return {};
}

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

      if (change.flags & kEvAdd) {
        // if (change.filter != kEvFiltDisplay && change.filter != kEvFiltGraphicsCore) {
        //   std::abort();
        // }

        kq->notes.push_back({
         .event = change,
         .enabled = (change.flags & kEvDisable) == 0
        });

        kq->notes.back().event.flags &= ~(kEvAdd | kEvClear | kEvDelete | kEvDisable);
      }

      // TODO
    }
  }

  std::vector<KEvent> result;
  result.reserve(nevents);

  for (auto &note : kq->notes) {
    if (result.size() >= nevents) {
      break;
    }

    if (!note.enabled) {
      continue;
    }

    if (note.event.filter == kEvFiltDisplay || note.event.filter == kEvFiltGraphicsCore) {
      result.push_back(note.event);
    }
  }

  std::memcpy(eventlist, result.data(), result.size() * sizeof(KEvent));
  thread->retval[0] = result.size();
  return {};
}
