#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_kqueue(Thread *thread) { return {}; }

orbis::SysResult orbis::sys_kevent(Thread *thread, sint fd,
                                   ptr<struct kevent> changelist, sint nchanges,
                                   ptr<struct kevent> eventlist, sint nevents,
                                   ptr<const struct timespec> timeout) {
  return {};
}
