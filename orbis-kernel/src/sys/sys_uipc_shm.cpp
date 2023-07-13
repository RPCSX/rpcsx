#include "orbis-config.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"

orbis::SysResult orbis::sys_shm_open(Thread *thread, ptr<const char> path,
                                     sint flags, mode_t mode) {
  char _name[256];
  if (auto result = ureadString(_name, sizeof(_name), path);
      result != ErrorCode{}) {
    return result;
  }

  ORBIS_LOG_TODO(__FUNCTION__, _name, flags, mode);
  return {};
}
orbis::SysResult orbis::sys_shm_unlink(Thread *thread, ptr<const char> path) {
  return ErrorCode::NOSYS;
}
