#include "sys/sysproto.hpp"

#include "thread/Process.hpp"
#include "utils/Logs.hpp"

struct KQueue : orbis::File {};

orbis::SysResult orbis::sys_kqueue(Thread *thread) {
  ORBIS_LOG_TODO(__FUNCTION__);
  auto queue = knew<KQueue>();
  if (queue == nullptr) {
    return ErrorCode::NOMEM;
  }

  thread->retval[0] = thread->tproc->fileDescriptors.insert(queue);
  return {};
}

orbis::SysResult orbis::sys_kevent(Thread *thread, sint fd,
                                   ptr<KEvent> changelist, sint nchanges,
                                   ptr<KEvent> eventlist, sint nevents,
                                   ptr<const timespec> timeout) {
  // ORBIS_LOG_TODO(__FUNCTION__, fd, changelist, nchanges, eventlist, nevents, timeout);
  thread->retval[0] = nevents;
  return {};
}
