#include "stat.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"
#include <filesystem>

orbis::SysResult orbis::sys_sync(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_quotactl(Thread *thread, ptr<char> path, sint cmd,
                                     sint uid, caddr_t arg) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_statfs(Thread *thread, ptr<char> path,
                                   ptr<struct statfs> buf) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fstatfs(Thread *thread, sint fd,
                                    ptr<struct statfs> buf) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getfsstat(Thread *thread, ptr<struct statfs> buf,
                                      slong bufsize, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fchdir(Thread *thread, sint fd) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_chdir(Thread *thread, ptr<char> path) {
  ORBIS_LOG_WARNING(__FUNCTION__, path);
  thread->tproc->cwd = std::filesystem::path(path).lexically_normal().string();
  return {};
}
orbis::SysResult orbis::sys_chroot(Thread *thread, ptr<char> path) {
  ORBIS_LOG_WARNING(__FUNCTION__, path);
  thread->tproc->root = path;
  return {};
}
orbis::SysResult orbis::sys_open(Thread *thread, ptr<char> path, sint flags,
                                 sint mode) {
  if (auto open = thread->tproc->ops->open) {
    Ref<File> file;
    auto result = open(thread, path, flags, mode, &file);
    if (result.isError()) {
      return result;
    }

    auto fd = thread->tproc->fileDescriptors.insert(file);
    thread->retval[0] = fd;
    ORBIS_LOG_NOTICE(__FUNCTION__, path, flags, mode, fd);
    return {};
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_openat(Thread *thread, sint fd, ptr<char> path,
                                   sint flag, mode_t mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mknod(Thread *thread, ptr<char> path, sint mode,
                                  sint dev) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mknodat(Thread *thread, sint fd, ptr<char> path,
                                    mode_t mode, dev_t dev) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mkfifo(Thread *thread, ptr<char> path, sint mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mkfifoat(Thread *thread, sint fd, ptr<char> path,
                                     mode_t mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_link(Thread *thread, ptr<char> path,
                                 ptr<char> link) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_linkat(Thread *thread, sint fd1, ptr<char> path1,
                                   sint fd2, ptr<char> path2, sint flag) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_symlink(Thread *thread, ptr<char> path,
                                    ptr<char> link) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_symlinkat(Thread *thread, ptr<char> path1, sint fd,
                                      ptr<char> path2) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_undelete(Thread *thread, ptr<char> path) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_unlink(Thread *thread, ptr<char> path) {
  if (auto unlink = thread->tproc->ops->unlink) {
    return unlink(thread, path);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_unlinkat(Thread *thread, sint fd, ptr<char> path,
                                     sint flag) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lseek(Thread *thread, sint fd, off_t offset,
                                  sint whence) {
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  std::lock_guard lock(file->mtx);

  // TODO: use ioctl?
  switch (whence) {
  case SEEK_SET:
    file->nextOff = offset;
    break;

  case SEEK_CUR:
    file->nextOff += offset;
    break;

  case SEEK_END: {
    if (file->ops->stat == nullptr) {
      ORBIS_LOG_ERROR("seek with end whence: unimplemented stat");
      return ErrorCode::NOTSUP;
    }

    orbis::Stat stat;
    auto result = file->ops->stat(file.get(), &stat, thread);
    if (result != ErrorCode{}) {
      return result;
    }

    file->nextOff = stat.size + offset;
    break;
  }

  default:
    ORBIS_LOG_ERROR("sys_lseek: unimplemented whence", whence);
    return ErrorCode::NOSYS;
  }

  thread->retval[0] = file->nextOff;
  return {};
}
orbis::SysResult orbis::sys_freebsd6_lseek(Thread *thread, sint fd, sint,
                                           off_t offset, sint whence) {
  return sys_lseek(thread, fd, offset, whence);
}
orbis::SysResult orbis::sys_access(Thread *thread, ptr<char> path, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_faccessat(Thread *thread, sint fd, ptr<char> path,
                                      sint mode, sint flag) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_eaccess(Thread *thread, ptr<char> path,
                                    sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_stat(Thread *thread, ptr<char> path, ptr<Stat> ub) {
  Ref<File> file;
  auto result = thread->tproc->ops->open(thread, path, 0, 0, &file);
  if (result.isError()) {
    return result;
  }

  auto stat = file->ops->stat;
  if (stat == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  Stat _ub;
  result = uread(_ub, ub);
  if (result.isError()) {
    return result;
  }

  result = stat(file.get(), &_ub, thread);
  if (result.isError()) {
    return result;
  }

  return uwrite(ub, _ub);
}
orbis::SysResult orbis::sys_fstatat(Thread *thread, sint fd, ptr<char> path,
                                    ptr<Stat> buf, sint flag) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lstat(Thread *thread, ptr<char> path,
                                  ptr<Stat> ub) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_nstat(Thread *thread, ptr<char> path,
                                  ptr<struct nstat> ub) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_nlstat(Thread *thread, ptr<char> path,
                                   ptr<struct nstat> ub) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_pathconf(Thread *thread, ptr<char> path,
                                     sint name) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lpathconf(Thread *thread, ptr<char> path,
                                      sint name) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_readlink(Thread *thread, ptr<char> path,
                                     ptr<char> buf, size_t count) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_readlinkat(Thread *thread, sint fd, ptr<char> path,
                                       ptr<char> buf, size_t bufsize) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_chflags(Thread *thread, ptr<char> path,
                                    sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lchflags(Thread *thread, ptr<const char> path,
                                     sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fchflags(Thread *thread, sint fd, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_chmod(Thread *thread, ptr<char> path, sint mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fchmodat(Thread *thread, sint fd, ptr<char> path,
                                     mode_t mode, sint flag) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lchmod(Thread *thread, ptr<char> path,
                                   mode_t mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fchmod(Thread *thread, sint fd, sint mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_chown(Thread *thread, ptr<char> path, sint uid,
                                  sint gid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fchownat(Thread *thread, sint fd, ptr<char> path,
                                     uid_t uid, gid_t gid, sint flag) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lchown(Thread *thread, ptr<char> path, sint uid,
                                   sint gid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fchown(Thread *thread, sint fd, sint uid,
                                   sint gid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_utimes(Thread *thread, ptr<char> path,
                                   ptr<struct timeval> tptr) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_futimesat(Thread *thread, sint fd, ptr<char> path,
                                      ptr<struct timeval> times) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lutimes(Thread *thread, ptr<char> path,
                                    ptr<struct timeval> tptr) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_futimes(Thread *thread, sint fd,
                                    ptr<struct timeval> tptr) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_truncate(Thread *thread, ptr<char> path,
                                     off_t length) {
  Ref<File> file;
  auto result = thread->tproc->ops->open(thread, path, 0, 0, &file);
  if (result.isError()) {
    return result;
  }

  auto truncate = file->ops->truncate;
  if (truncate == nullptr) {
    return ErrorCode::NOTSUP;
  }

  std::lock_guard lock(file->mtx);
  return truncate(file.get(), length, thread);
}
orbis::SysResult orbis::sys_freebsd6_truncate(Thread *thread, ptr<char> path,
                                              sint, off_t length) {
  return sys_truncate(thread, path, length);
}
orbis::SysResult orbis::sys_fsync(Thread *thread, sint fd) { return {}; }
orbis::SysResult orbis::sys_rename(Thread *thread, ptr<char> from,
                                   ptr<char> to) {
  if (auto rename = thread->tproc->ops->rename) {
    return rename(thread, from, to);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_renameat(Thread *thread, sint oldfd, ptr<char> old,
                                     sint newfd, ptr<char> new_) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mkdir(Thread *thread, ptr<char> path, sint mode) {
  if (auto mkdir = thread->tproc->ops->mkdir) {
    return mkdir(thread, path, mode);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mkdirat(Thread *thread, sint fd, ptr<char> path,
                                    mode_t mode) {
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  auto mkdir = file->ops->mkdir;

  if (mkdir == nullptr) {
    return ErrorCode::NOTSUP;
  }
  std::lock_guard lock(file->mtx);

  return mkdir(file.get(), path, mode);
}

orbis::SysResult orbis::sys_rmdir(Thread *thread, ptr<char> path) {
  if (auto rmdir = thread->tproc->ops->rmdir) {
    return rmdir(thread, path);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getdirentries(Thread *thread, sint fd,
                                          ptr<char> buf, uint count,
                                          ptr<slong> basep) {
  ORBIS_LOG_WARNING(__FUNCTION__, fd, (void *)buf, count, basep);
  Ref<File> file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  std::lock_guard lock(file->mtx);
  const orbis::Dirent *entries = file->dirEntries.data();
  auto pos = file->nextOff / sizeof(orbis::Dirent); // TODO
  auto rmax = count / sizeof(orbis::Dirent);
  auto next = std::min<uint64_t>(file->dirEntries.size(), pos + rmax);
  file->nextOff = next * sizeof(orbis::Dirent);
  SysResult result{};
  result = uwrite((orbis::Dirent *)buf, entries + pos, next - pos);
  if (result.isError())
    return result;

  if (basep) {
    result = uwrite(basep, slong(file->nextOff));
    if (result.isError())
      return result;
  }

  thread->retval[0] = (next - pos) * sizeof(orbis::Dirent);
  return {};
}
orbis::SysResult orbis::sys_getdents(Thread *thread, sint fd, ptr<char> buf,
                                     size_t count) {
  ORBIS_LOG_WARNING(__FUNCTION__, fd, (void *)buf, count);
  return orbis::sys_getdirentries(thread, fd, buf, count, nullptr);
}
orbis::SysResult orbis::sys_umask(Thread *thread, sint newmask) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_revoke(Thread *thread, ptr<char> path) {
  ORBIS_LOG_WARNING(__FUNCTION__);
  return {};
}
orbis::SysResult orbis::sys_lgetfh(Thread *thread, ptr<char> fname,
                                   ptr<struct fhandle> fhp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getfh(Thread *thread, ptr<char> fname,
                                  ptr<struct fhandle> fhp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_fhopen(Thread *thread, ptr<const struct fhandle> u_fhp, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fhstat(Thread *thread,
                                   ptr<const struct fhandle> u_fhp,
                                   ptr<Stat> sb) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fhstatfs(Thread *thread,
                                     ptr<const struct fhandle> u_fhp,
                                     ptr<struct statfs> buf) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_posix_fallocate(Thread *thread, sint fd,
                                            off_t offset, off_t len) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_posix_fadvise(Thread *thread, sint fd, off_t offset,
                                          off_t len, sint advice) {
  return ErrorCode::NOSYS;
}
