#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/Thread.hpp"
#include <mutex>
#include <string>

orbis::SysResult orbis::sys___getcwd(Thread *thread, ptr<char> buf,
                                     uint buflen) {

  std::string cwd;

  {
    std::lock_guard lock(thread->tproc->mtx);
    cwd = std::string(thread->tproc->cwd);
  }

  if (buflen < cwd.size() + 1) {
    return ErrorCode::NOMEM;
  }

  ORBIS_RET_ON_ERROR(uwriteRaw(buf, cwd.data(), cwd.size() + 1));
  return {};
}
