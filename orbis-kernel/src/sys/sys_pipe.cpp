#include "sys/sysproto.hpp"
#include "thread/Thread.hpp"
#include "thread/Process.hpp"
#include "utils/Logs.hpp"
#include <pipe.hpp>

orbis::SysResult orbis::sys_pipe(Thread *thread) {
  auto [a, b] = createPipe();
  auto fd0 = thread->tproc->fileDescriptors.insert(a);
  auto fd1 = thread->tproc->fileDescriptors.insert(b);
  ORBIS_LOG_ERROR(__FUNCTION__, fd0, fd1);
  thread->retval[0] = fd0;
  thread->retval[1] = fd1;
  return {};
}
