#include "file.hpp"
#include "orbis/utils/Logs.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/Thread.hpp"
#include "uio.hpp"
#include <sstream>

orbis::SysResult orbis::sys_read(Thread *thread, sint fd, ptr<void> buf,
                                 size_t nbyte) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto read = file->ops->read;
  if (read == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  IoVec vec{.base = (void *)buf, .len = nbyte};

  Uio io{
      .offset = file->nextOff,
      .iov = &vec,
      .iovcnt = 1,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Read,
      .td = thread,
  };

  auto error = read(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    if (error == ErrorCode::BUSY) {
      return SysResult::notAnError(error);
    }

    return error;
  }

  auto cnt = io.offset - file->nextOff;
  file->nextOff = io.offset;

  // ORBIS_LOG_ERROR(__FUNCTION__, fd, buf, nbyte, cnt);
  thread->retval[0] = cnt;
  return {};
}
orbis::SysResult orbis::sys_pread(Thread *thread, sint fd, ptr<void> buf,
                                  size_t nbyte, off_t offset) {
  // ORBIS_LOG_ERROR(__FUNCTION__, fd, buf, nbyte, offset);
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto read = file->ops->read;
  if (read == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  IoVec vec{.base = (void *)buf, .len = nbyte};

  Uio io{
      .offset = static_cast<std::uint64_t>(offset),
      .iov = &vec,
      .iovcnt = 1,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Read,
      .td = thread,
  };

  auto error = read(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    return error;
  }

  thread->retval[0] = io.offset - offset;
  return {};
}
orbis::SysResult orbis::sys_freebsd6_pread(Thread *thread, sint fd,
                                           ptr<void> buf, size_t nbyte, sint,
                                           off_t offset) {
  return sys_pread(thread, fd, buf, nbyte, offset);
}
orbis::SysResult orbis::sys_readv(Thread *thread, sint fd, ptr<IoVec> iovp,
                                  uint iovcnt) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto read = file->ops->read;
  if (read == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);

  Uio io{
      .offset = file->nextOff,
      .iov = iovp,
      .iovcnt = iovcnt,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Read,
      .td = thread,
  };

  auto error = read(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    return error;
  }

  auto cnt = io.offset - file->nextOff;
  file->nextOff = io.offset;
  thread->retval[0] = cnt;
  return {};
}
orbis::SysResult orbis::sys_preadv(Thread *thread, sint fd, ptr<IoVec> iovp,
                                   uint iovcnt, off_t offset) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto read = file->ops->read;
  if (read == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);

  Uio io{
      .offset = static_cast<std::uint64_t>(offset),
      .iov = iovp,
      .iovcnt = iovcnt,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Read,
      .td = thread,
  };

  auto error = read(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    return error;
  }

  thread->retval[0] = io.offset - offset;
  return {};
}
orbis::SysResult orbis::sys_write(Thread *thread, sint fd, ptr<const void> buf,
                                  size_t nbyte) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto write = file->ops->write;
  if (write == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  IoVec vec{.base = (void *)buf, .len = nbyte};

  Uio io{
      .offset = file->nextOff,
      .iov = &vec,
      .iovcnt = 1,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Write,
      .td = thread,
  };

  auto error = write(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    return error;
  }

  auto cnt = io.offset - file->nextOff;
  file->nextOff = io.offset;

  thread->retval[0] = cnt;
  return {};
}
orbis::SysResult orbis::sys_pwrite(Thread *thread, sint fd, ptr<const void> buf,
                                   size_t nbyte, off_t offset) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto write = file->ops->write;
  if (write == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  IoVec vec{.base = (void *)buf, .len = nbyte};

  Uio io{
      .offset = static_cast<std::uint64_t>(offset),
      .iov = &vec,
      .iovcnt = 1,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Write,
      .td = thread,
  };

  auto error = write(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    return error;
  }

  thread->retval[0] = io.offset - offset;
  return {};
}
orbis::SysResult orbis::sys_freebsd6_pwrite(Thread *thread, sint fd,
                                            ptr<const void> buf, size_t nbyte,
                                            sint, off_t offset) {
  return sys_pwrite(thread, fd, buf, nbyte, offset);
}
orbis::SysResult orbis::sys_writev(Thread *thread, sint fd, ptr<IoVec> iovp,
                                   uint iovcnt) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto write = file->ops->write;
  if (write == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);

  Uio io{
      .offset = file->nextOff,
      .iov = iovp,
      .iovcnt = iovcnt,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Write,
      .td = thread,
  };

  auto error = write(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    return error;
  }

  auto cnt = io.offset - file->nextOff;
  file->nextOff = io.offset;

  thread->retval[0] = cnt;
  return {};
}
orbis::SysResult orbis::sys_pwritev(Thread *thread, sint fd, ptr<IoVec> iovp,
                                    uint iovcnt, off_t offset) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto write = file->ops->write;
  if (write == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);

  Uio io{
      .offset = static_cast<std::uint64_t>(offset),
      .iov = iovp,
      .iovcnt = iovcnt,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Write,
      .td = thread,
  };
  auto error = write(file.get(), &io, thread);
  if (error != ErrorCode{} && error != ErrorCode::AGAIN) {
    return error;
  }

  thread->retval[0] = io.offset - offset;
  return {};
}
orbis::SysResult orbis::sys_ftruncate(Thread *thread, sint fd, off_t length) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto truncate = file->ops->truncate;
  if (truncate == nullptr) {
    return ErrorCode::NOTSUP;
  }

  ORBIS_LOG_WARNING(__FUNCTION__, fd, length);
  std::lock_guard lock(file->mtx);
  return truncate(file.get(), length, thread);
}
orbis::SysResult orbis::sys_freebsd6_ftruncate(Thread *thread, sint fd, sint,
                                               off_t length) {
  return sys_ftruncate(thread, fd, length);
}
orbis::SysResult orbis::sys_ioctl(Thread *thread, sint fd, ulong com,
                                  caddr_t data) {
  rx::Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto ioctl = file->ops->ioctl;
  if (ioctl == nullptr) {
    if (auto device = file->device.get()) {
      return device->ioctl(com, data, thread);
    }

    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  return ioctl(file.get(), com, data, thread);
}
orbis::SysResult orbis::sys_pselect(Thread *thread, sint nd, ptr<fd_set_t> in,
                                    ptr<fd_set_t> ou, ptr<fd_set_t> ex,
                                    ptr<const timespec> ts,
                                    ptr<const SigSet> sm) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_select(Thread *thread, sint nd,
                                   ptr<struct fd_set_t> in,
                                   ptr<struct fd_set_t> out,
                                   ptr<struct fd_set_t> ex,
                                   ptr<struct timeval> tv) {
  if (tv == nullptr) {
    orbis::scoped_unblock_now unblock;
    std::this_thread::sleep_for(std::chrono::days(1));
  } else {
    std::this_thread::sleep_for(std::chrono::seconds(tv->tv_sec));
    return orbis::ErrorCode::TIMEDOUT;
  }

  return {};
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
    ORBIS_LOG_WARNING("sys_sysarch: set FS", (std::size_t)fs);
    thread->fsBase = fs;
    return {};
  }

  ORBIS_LOG_WARNING(__FUNCTION__, op, parms);
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
