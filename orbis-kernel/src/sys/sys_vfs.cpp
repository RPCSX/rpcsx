#include "stat.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"

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
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_chroot(Thread *thread, ptr<char> path) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_open(Thread *thread, ptr<char> path, sint flags,
                                 sint mode) {
  ORBIS_LOG_NOTICE("sys_open", path, flags, mode);
  if (auto open = thread->tproc->ops->open) {
    return open(thread, path, flags, mode);
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
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_unlinkat(Thread *thread, sint fd, ptr<char> path,
                                     sint flag) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_lseek(Thread *thread, sint fd, off_t offset,
                                  sint whence) {
  if (auto lseek = thread->tproc->ops->lseek) {
    return lseek(thread, fd, offset, whence);
  }
  return ErrorCode::NOSYS;
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
  ORBIS_LOG_WARNING(__FUNCTION__, path, ub);
  auto result = sys_open(thread, path, 0, 0);
  if (result.isError()) {
    return ErrorCode::NOENT;
  }

  auto fd = thread->retval[0];
  result = sys_lseek(thread, fd, 0, SEEK_END);
  if (result.isError()) {
    sys_close(thread, fd);
    return result;
  }

  auto len = thread->retval[0];
  result = sys_pread(thread, fd, ub, 1, 0);
  if (result.isError()) {
    *ub = {};
    ub->mode = 0777 | 0x4000;
  } else {
    *ub = {};
    ub->size = len;
    ub->blksize = 1;
    ub->blocks = len;
    ub->mode = 0777 | 0x8000;
  }

  sys_close(thread, fd);
  thread->retval[0] = 0;
  return {};
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
  if (auto truncate = thread->tproc->ops->truncate) {
    return truncate(thread, path, length);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_freebsd6_truncate(Thread *thread, ptr<char> path,
                                              sint, off_t length) {
  return sys_truncate(thread, path, length);
}
orbis::SysResult orbis::sys_fsync(Thread *thread, sint fd) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_rename(Thread *thread, ptr<char> from,
                                   ptr<char> to) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_renameat(Thread *thread, sint oldfd, ptr<char> old,
                                     sint newfd, ptr<char> new_) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mkdir(Thread *thread, ptr<char> path, sint mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mkdirat(Thread *thread, sint fd, ptr<char> path,
                                    mode_t mode) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_rmdir(Thread *thread, ptr<char> path) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getdirentries(Thread *thread, sint fd,
                                          ptr<char> buf, uint count,
                                          ptr<slong> basep) {
  ORBIS_LOG_ERROR(__FUNCTION__, fd, (void *)buf, count, basep);
  thread->where();
  return {};
}
orbis::SysResult orbis::sys_getdents(Thread *thread, sint fd, ptr<char> buf,
                                     size_t count) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_umask(Thread *thread, sint newmask) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_revoke(Thread *thread, ptr<char> path) {
  return ErrorCode::NOSYS;
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
