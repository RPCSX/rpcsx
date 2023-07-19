#include "orbis/utils/Logs.hpp"
#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_read(Thread *thread, sint fd, ptr<void> buf,
                                 size_t nbyte) {
  ORBIS_LOG_TRACE(__FUNCTION__, fd, buf, nbyte);
  if (auto read = thread->tproc->ops->read) {
    return read(thread, fd, buf, nbyte);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_pread(Thread *thread, sint fd, ptr<void> buf,
                                  size_t nbyte, off_t offset) {
  ORBIS_LOG_TRACE(__FUNCTION__, fd, buf, nbyte, offset);
  if (auto pread = thread->tproc->ops->pread) {
    return pread(thread, fd, buf, nbyte, offset);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_freebsd6_pread(Thread *thread, sint fd,
                                           ptr<void> buf, size_t nbyte, sint,
                                           off_t offset) {
  return sys_pread(thread, fd, buf, nbyte, offset);
}
orbis::SysResult orbis::sys_readv(Thread *thread, sint fd,
                                  ptr<struct iovec> iovp, uint iovcnt) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_preadv(Thread *thread, sint fd,
                                   ptr<struct iovec> iovp, uint iovcnt,
                                   off_t offset) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_write(Thread *thread, sint fd, ptr<const void> buf,
                                  size_t nbyte) {
  ORBIS_LOG_NOTICE(__FUNCTION__, fd, buf, nbyte);
  if (auto write = thread->tproc->ops->write) {
    return write(thread, fd, buf, nbyte);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_pwrite(Thread *thread, sint fd, ptr<const void> buf,
                                   size_t nbyte, off_t offset) {
  if (auto pwrite = thread->tproc->ops->pwrite) {
    return pwrite(thread, fd, buf, nbyte, offset);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_freebsd6_pwrite(Thread *thread, sint fd,
                                            ptr<const void> buf, size_t nbyte,
                                            sint, off_t offset) {
  return sys_pwrite(thread, fd, buf, nbyte, offset);
}
orbis::SysResult orbis::sys_writev(Thread *thread, sint fd,
                                   ptr<struct iovec> iovp, uint iovcnt) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_pwritev(Thread *thread, sint fd,
                                    ptr<struct iovec> iovp, uint iovcnt,
                                    off_t offset) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ftruncate(Thread *thread, sint fd, off_t length) {
  if (auto ftruncate = thread->tproc->ops->ftruncate) {
    return ftruncate(thread, fd, length);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_freebsd6_ftruncate(Thread *thread, sint fd, sint,
                                               off_t length) {
  return sys_ftruncate(thread, fd, length);
}
orbis::SysResult orbis::sys_ioctl(Thread *thread, sint fd, ulong com,
                                  caddr_t data) {
  ORBIS_LOG_WARNING(__FUNCTION__, fd, com);
  if (auto ioctl = thread->tproc->ops->ioctl) {
    return ioctl(thread, fd, com, data);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_pselect(Thread *thread, sint nd, ptr<fd_set> in,
                                    ptr<fd_set> ou, ptr<fd_set> ex,
                                    ptr<const timespec> ts,
                                    ptr<const sigset_t> sm) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_select(Thread *thread, sint nd,
                                   ptr<struct fd_set_t> in,
                                   ptr<struct fd_set_t> out,
                                   ptr<struct fd_set_t> ex,
                                   ptr<struct timeval> tv) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_poll(Thread *thread, ptr<struct pollfd> fds,
                                 uint nfds, sint timeout) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_openbsd_poll(Thread *thread, ptr<struct pollfd> fds,
                                         uint nfds, sint timeout) {
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_nlm_syscall(Thread *thread, sint debug_level,
                                        sint grace_period, sint addr_count,
                                        ptr<ptr<char>> addrs) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_nfssvc(Thread *thread, sint flag, caddr_t argp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sysarch(Thread *thread, sint op, ptr<char> parms) {
  if (op == 129) {
    uint64_t fs;
    if (auto error = uread(fs, (ptr<uint64_t>)parms); error != ErrorCode{})
      return error;
    std::printf("sys_sysarch: set FS to 0x%zx\n", (std::size_t)fs);
    thread->fsBase = fs;
    return {};
  }

  std::printf("sys_sysarch(op = %d, parms = %p): unknown op\n", op, parms);
  return {};
}
orbis::SysResult orbis::sys_nnpfs_syscall(Thread *thread, sint operation,
                                          ptr<char> a_pathP, sint opcode,
                                          ptr<void> a_paramsP,
                                          sint a_followSymlinks) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_afs3_syscall(Thread *thread, slong syscall,
                                         slong param1, slong param2,
                                         slong param3, slong param4,
                                         slong param5, slong param6) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_gssd_syscall(Thread *thread, ptr<char> path) {
  return ErrorCode::NOSYS;
}
