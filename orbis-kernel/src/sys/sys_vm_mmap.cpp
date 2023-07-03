#include "error.hpp"
#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_sbrk(Thread *, sint) {
  return ErrorCode::OPNOTSUPP;
}
orbis::SysResult orbis::sys_sstk(Thread *, sint) {
  return ErrorCode::OPNOTSUPP;
}

orbis::SysResult orbis::sys_mmap(Thread *thread, caddr_t addr, size_t len,
                                 sint prot, sint flags, sint fd, off_t pos) {
  if (auto impl = thread->tproc->ops->mmap) {
    return impl(thread, addr, len, prot, flags, fd, pos);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_freebsd6_mmap(Thread *thread, caddr_t addr,
                                          size_t len, sint prot, sint flags,
                                          sint fd, sint, off_t pos) {
  return sys_mmap(thread, addr, len, prot, flags, fd, pos);
}
orbis::SysResult orbis::sys_msync(Thread *thread, ptr<void> addr, size_t len,
                                  sint flags) {
  if (auto impl = thread->tproc->ops->msync) {
    return impl(thread, addr, len, flags);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_munmap(Thread *thread, ptr<void> addr, size_t len) {
  if (auto impl = thread->tproc->ops->munmap) {
    return impl(thread, addr, len);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mprotect(Thread *thread, ptr<const void> addr,
                                     size_t len, sint prot) {
  if (auto impl = thread->tproc->ops->mprotect) {
    return impl(thread, addr, len, prot);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_minherit(Thread *thread, ptr<void> addr, size_t len,
                                     sint inherit) {
  if (auto impl = thread->tproc->ops->minherit) {
    return impl(thread, addr, len, inherit);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_madvise(Thread *thread, ptr<void> addr, size_t len,
                                    sint behav) {
  if (auto impl = thread->tproc->ops->madvise) {
    return impl(thread, addr, len, behav);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mincore(Thread *thread, ptr<const void> addr,
                                    size_t len, ptr<char> vec) {
  if (auto impl = thread->tproc->ops->mincore) {
    return impl(thread, addr, len, vec);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mlock(Thread *thread, ptr<const void> addr,
                                  size_t len) {
  if (auto impl = thread->tproc->ops->mlock) {
    return impl(thread, addr, len);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mlockall(Thread *thread, sint how) {
  if (auto impl = thread->tproc->ops->mlockall) {
    return impl(thread, how);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_munlockall(Thread *thread) {
  if (auto impl = thread->tproc->ops->munlockall) {
    return impl(thread);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_munlock(Thread *thread, ptr<const void> addr,
                                    size_t len) {
   if (auto impl = thread->tproc->ops->munlock) {
    return impl(thread, addr, len);
  }

  return ErrorCode::NOSYS;
}
