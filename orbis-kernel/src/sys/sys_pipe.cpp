#include "sys/sysproto.hpp"
#include <pipe.hpp>

orbis::SysResult orbis::sys_pipe(Thread *thread) {
  auto pipe = createPipe();
  thread->retval[0] = thread->tproc->fileDescriptors.insert(pipe);
  thread->retval[1] = thread->tproc->fileDescriptors.insert(pipe);
  return {};
}
