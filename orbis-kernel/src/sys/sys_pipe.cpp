#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"
#include <pipe.hpp>

orbis::SysResult orbis::sys_pipe(Thread *thread) {
  auto pipe = createPipe();
  auto fd0 = thread->tproc->fileDescriptors.insert(pipe);
  auto fd1 = thread->tproc->fileDescriptors.insert(pipe);
  ORBIS_LOG_ERROR(__FUNCTION__, fd0, fd1);
  thread->retval[0] = fd0;
  thread->retval[1] = fd1;
  return {};
}
