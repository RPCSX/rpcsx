#include "file.hpp"
#include "orbis-config.hpp"
#include "sys/sysproto.hpp"
#include "thread/ProcessOps.hpp"
#include "thread/Thread.hpp"
#include "thread/Process.hpp"

orbis::SysResult orbis::sys_shm_open(Thread *thread, ptr<const char> path,
                                     sint flags, mode_t mode) {
  char _name[256];
  if (auto result = ureadString(_name, sizeof(_name), path);
      result != ErrorCode{}) {
    return result;
  }

  if (auto shm_open = thread->tproc->ops->shm_open) {
    Ref<File> file;
    auto result = shm_open(thread, path, flags, mode, &file);
    if (result.isError()) {
      return result;
    }

    thread->retval[0] = thread->tproc->fileDescriptors.insert(file);
    return {};
  }
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_shm_unlink(Thread *thread, ptr<const char> path) {
  char _name[256];
  if (auto result = ureadString(_name, sizeof(_name), path);
      result != ErrorCode{}) {
    return result;
  }

  if (auto shm_unlink = thread->tproc->ops->shm_unlink) {
    auto result = shm_unlink(thread, path);
    if (result.isError()) {
      return result;
    }
    return {};
  }

  return ErrorCode::NOSYS;
}
