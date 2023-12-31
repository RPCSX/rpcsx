#include "orbis/utils/Logs.hpp"
#include "sys/sysproto.hpp"
#include "uio.hpp"
#include <sstream>

orbis::SysResult orbis::sys_read(Thread *thread, sint fd, ptr<void> buf,
                                 size_t nbyte) {
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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

  thread->retval[0] = cnt;
  return {};
}
orbis::SysResult orbis::sys_pread(Thread *thread, sint fd, ptr<void> buf,
                                  size_t nbyte, off_t offset) {
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
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
// clang-format off
#define IOCPARM_SHIFT 13 /* number of bits for ioctl size */
#define IOCPARM_MASK ((1 << IOCPARM_SHIFT) - 1) /* parameter length mask */
#define IOCPARM_LEN(x) (((x) >> 16) & IOCPARM_MASK)
#define IOCBASECMD(x) ((x) & ~(IOCPARM_MASK << 16))
#define IOCGROUP(x) (((x) >> 8) & 0xff)

#define IOCPARM_MAX (1 << IOCPARM_SHIFT) /* max size of ioctl */
#define IOC_VOID 0x20000000              /* no parameters */
#define IOC_OUT 0x40000000               /* copy out parameters */
#define IOC_IN 0x80000000                /* copy in parameters */
#define IOC_INOUT (IOC_IN | IOC_OUT)
#define IOC_DIRMASK (IOC_VOID | IOC_OUT | IOC_IN)

#define _IOC(inout, group, num, len)                                           \
  ((unsigned long)((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | \
                   (num)))
#define _IO(g, n) _IOC(IOC_VOID, (g), (n), 0)
#define _IOWINT(g, n) _IOC(IOC_VOID, (g), (n), sizeof(int))
#define _IOR(g, n, t) _IOC(IOC_OUT, (g), (n), sizeof(t))
#define _IOW(g, n, t) _IOC(IOC_IN, (g), (n), sizeof(t))
/* this should be _IORW, but stdio got there first */
#define _IOWR(g, n, t) _IOC(IOC_INOUT, (g), (n), sizeof(t))
// clang-format on

static std::string iocGroupToString(unsigned iocGroup) {
  if (iocGroup >= 128) {
    const char *sceGroups[] = {
        "DEV",
        "DMEM",
        "GC",
        "DCE",
        "UVD",
        "VCE",
        "DBGGC",
        "TWSI",
        "MDBG",
        "DEVENV",
        "AJM",
        "TRACE",
        "IBS",
        "MBUS",
        "HDMI",
        "CAMERA",
        "FAN",
        "THERMAL",
        "PFS",
        "ICC_CONFIG",
        "IPC",
        "IOSCHED",
        "ICC_INDICATOR",
        "EXFATFS",
        "ICC_NVS",
        "DVE",
        "ICC_POWER",
        "AV_CONTROL",
        "ICC_SC_CONFIGURATION",
        "ICC_DEVICE_POWER",
        "SSHOT",
        "DCE_SCANIN",
        "FSCTRL",
        "HMD",
        "SHM",
        "PHYSHM",
        "HMDDFU",
        "BLUETOOTH_HID",
        "SBI",
        "S3DA",
        "SPM",
        "BLOCKPOOL",
        "SDK_EVENTLOG",
    };

    if (iocGroup - 127 >= std::size(sceGroups)) {
      return "'?'";
    }

    return sceGroups[iocGroup - 127];
  }

  if (isprint(iocGroup)) {
    return "'" + std::string(1, (char)iocGroup) + "'";
  }

  return "'?'";
}

static void printIoctl(unsigned long arg) {
  std::printf("0x%lx { IO%s%s %lu(%s), %lu, %lu }\n", arg,
              arg & IOC_OUT ? "R" : "", arg & IOC_IN ? "W" : "", IOCGROUP(arg),
              iocGroupToString(IOCGROUP(arg)).c_str(), arg & 0xFF,
              IOCPARM_LEN(arg));
}

static void ioctlToStream(std::ostream &stream, unsigned long arg) {
  stream << "0x" << std::hex << arg << " { IO";

  if ((arg & IOC_OUT) != 0) {
    stream << 'R';
  }

  if ((arg & IOC_IN) != 0) {
    stream << 'W';
  }
  if ((arg & IOC_VOID) != 0) {
    stream << 'i';
  }

  stream << " 0x" << IOCGROUP(arg);
  stream << "('" << iocGroupToString(IOCGROUP(arg)) << "'), ";
  stream << std::dec << (arg & 0xFF) << ", " << IOCPARM_LEN(arg) << " }";
}

static std::string ioctlToString(unsigned long arg) {
  std::ostringstream stream;
  ioctlToStream(stream, arg);
  return std::move(stream).str();
}

orbis::SysResult orbis::sys_ioctl(Thread *thread, sint fd, ulong com,
                                  caddr_t data) {
  auto str = ioctlToString(com);
  // ORBIS_LOG_WARNING(__FUNCTION__, fd, com, str);
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto ioctl = file->ops->ioctl;
  if (ioctl == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  return ioctl(file.get(), com, data, thread);
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
