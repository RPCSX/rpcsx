#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_cpuset(Thread *thread, ptr<cpusetid_t> setid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cpuset_setid(Thread *thread, cpuwhich_t which,
                                         id_t id, cpusetid_t setid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cpuset_getid(Thread *thread, cpulevel_t level,
                                         cpuwhich_t which, id_t id,
                                         ptr<cpusetid_t> setid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cpuset_getaffinity(Thread *thread, cpulevel_t level,
                                               cpuwhich_t which, id_t id,
                                               size_t cpusetsize,
                                               ptr<cpuset> mask) {
  return {};
}
orbis::SysResult orbis::sys_cpuset_setaffinity(Thread *thread, cpulevel_t level,
                                               cpuwhich_t which, id_t id,
                                               size_t cpusetsize,
                                               ptr<const cpuset> mask) {
  return {};
}
