#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/stat.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"
#include <cerrno>
#include <fcntl.h>
#include <span>
#include <string>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

struct HostFile : orbis::File {
  int hostFd = -1;

  ~HostFile() {
    if (hostFd > 0) {
      ::close(hostFd);
    }
  }
};

struct SocketFile : orbis::File {
  orbis::utils::kstring name;
  int hostFd = -1;

  ~SocketFile() {
    if (hostFd > 0) {
      ::close(hostFd);
    }
  }
};

struct HostFsDevice : IoDevice {
  orbis::kstring hostPath;

  explicit HostFsDevice(orbis::kstring path) : hostPath(std::move(path)) {}
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode convertErrno() {
  switch (auto error = errno) {
  case EPERM:
    return orbis::ErrorCode::PERM;
  case ENOENT:
    return orbis::ErrorCode::NOENT;
  case EBADF:
    return orbis::ErrorCode::BADF;
  case EIO:
    return orbis::ErrorCode::IO;
  case EACCES:
    return orbis::ErrorCode::ACCES;
  case EEXIST:
    return orbis::ErrorCode::EXIST;
  case EBUSY:
    return orbis::ErrorCode::BUSY;
  case ENOTDIR:
    return orbis::ErrorCode::NOTDIR;
  case EISDIR:
    return orbis::ErrorCode::ISDIR;
  case EFBIG:
    return orbis::ErrorCode::FBIG;
  case ENOSPC:
    return orbis::ErrorCode::NOSPC;
  case ESPIPE:
    return orbis::ErrorCode::SPIPE;
  case EPIPE:
    return orbis::ErrorCode::PIPE;
  case EINVAL:
    return orbis::ErrorCode::INVAL;

  default:
    ORBIS_LOG_ERROR("Unconverted errno", error);
  }

  return orbis::ErrorCode::IO;
}

static orbis::ErrorCode host_read(orbis::File *file, orbis::Uio *uio,
                                  orbis::Thread *) {
  auto hostFile = static_cast<HostFile *>(file);
  std::vector<iovec> vec;
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    vec.push_back({.iov_base = entry.base, .iov_len = entry.len});
  }

  ssize_t cnt = 0;
  if (hostFile->hostFd == 0) {
    for (auto io : vec) {
      cnt += ::read(hostFile->hostFd, io.iov_base, io.iov_len);

      if (cnt != io.iov_len) {
        break;
      }
    }
  } else {
    cnt = ::preadv(hostFile->hostFd, vec.data(), vec.size(), uio->offset);
  }
  if (cnt < 0) {
    return convertErrno();
  }

  uio->resid -= cnt;
  uio->offset += cnt;
  return {};
}

static orbis::ErrorCode host_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *) {
  auto hostFile = static_cast<HostFile *>(file);
  std::vector<iovec> vec;
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    vec.push_back({.iov_base = entry.base, .iov_len = entry.len});
  }

  ssize_t cnt = 0;
  if (hostFile->hostFd == 1 || hostFile->hostFd == 2) {
    for (auto io : vec) {
      cnt += ::write(hostFile->hostFd, io.iov_base, io.iov_len);

      if (cnt != io.iov_len) {
        break;
      }
    }
  } else {
    cnt = ::pwritev(hostFile->hostFd, vec.data(), vec.size(), uio->offset);
  }
  if (cnt < 0) {
    return convertErrno();
  }

  uio->resid -= cnt;
  uio->offset += cnt;
  return {};
}

static orbis::ErrorCode host_mmap(orbis::File *file, void **address,
                                  std::uint64_t size, std::int32_t prot,
                                  std::int32_t flags, std::int64_t offset,
                                  orbis::Thread *thread) {
  auto hostFile = static_cast<HostFile *>(file);

  auto result =
      rx::vm::map(*address, size, prot, flags, rx::vm::kMapInternalReserveOnly);

  if (result == (void *)-1) {
    return orbis::ErrorCode::NOMEM;
  }

  result = ::mmap(result, size, prot & rx::vm::kMapProtCpuAll,
                  MAP_SHARED | MAP_FIXED, hostFile->hostFd, offset);
  if (result == (void *)-1) {
    auto result = convertErrno();
    return result;
  }

  std::printf("shm mapped at %p-%p\n", result, (char *)result + size);

  *address = result;
  return {};
}

static orbis::ErrorCode host_stat(orbis::File *file, orbis::Stat *sb,
                                  orbis::Thread *thread) {
  auto hostFile = static_cast<HostFile *>(file);
  struct stat hostStat;
  ::fstat(hostFile->hostFd, &hostStat);
  sb->dev = hostStat.st_dev; // TODO
  sb->ino = hostStat.st_ino;
  sb->mode = hostStat.st_mode;
  sb->nlink = hostStat.st_nlink;
  sb->uid = hostStat.st_uid;
  sb->gid = hostStat.st_gid;
  sb->rdev = hostStat.st_rdev;
  sb->atim = {
      .sec = static_cast<uint64_t>(hostStat.st_atim.tv_sec),
      .nsec = static_cast<uint64_t>(hostStat.st_atim.tv_nsec),
  };
  sb->mtim = {
      .sec = static_cast<uint64_t>(hostStat.st_mtim.tv_sec),
      .nsec = static_cast<uint64_t>(hostStat.st_mtim.tv_nsec),
  };
  sb->ctim = {
      .sec = static_cast<uint64_t>(hostStat.st_mtim.tv_sec),
      .nsec = static_cast<uint64_t>(hostStat.st_mtim.tv_nsec),
  };
  sb->size = hostStat.st_size;
  sb->blocks = hostStat.st_blocks;
  sb->blksize = hostStat.st_blksize;
  // TODO
  sb->flags = 0;
  sb->gen = 0;
  sb->lspare = 0;
  sb->birthtim = {
      .sec = static_cast<uint64_t>(hostStat.st_mtim.tv_sec),
      .nsec = static_cast<uint64_t>(hostStat.st_mtim.tv_nsec),
  };

  return {};
}

static orbis::ErrorCode host_truncate(orbis::File *file, std::uint64_t len,
                                      orbis::Thread *thread) {
  auto hostFile = static_cast<HostFile *>(file);
  if (::ftruncate(hostFile->hostFd, len)) {
    return convertErrno();
  }

  return {};
}

static orbis::ErrorCode socket_ioctl(orbis::File *file, std::uint64_t request,
                                     void *argp, orbis::Thread *thread) {
  auto soc = static_cast<SocketFile *>(file);
  ORBIS_LOG_FATAL("Unhandled socket ioctl", request, soc->name);
  return {};
}

static const orbis::FileOps hostOps = {
    .read = host_read,
    .write = host_write,
    .truncate = host_truncate,
    .stat = host_stat,
    .mmap = host_mmap,
};

static const orbis::FileOps socketOps = {
    .ioctl = socket_ioctl,
};

IoDevice *createHostIoDevice(orbis::kstring hostPath) {
  return orbis::knew<HostFsDevice>(std::move(hostPath));
}

orbis::ErrorCode createSocket(orbis::Ref<orbis::File> *file,
                              orbis::kstring name, int dom, int type,
                              int prot) {
  auto fd = ::socket(dom, type, prot);
  if (fd < 0) {
    return convertErrno();
  }

  auto s = orbis::knew<SocketFile>();
  s->name = std::move(name);
  s->hostFd = fd;
  s->ops = &socketOps;
  *file = s;
  return {};
}

orbis::ErrorCode HostFsDevice::open(orbis::Ref<orbis::File> *file,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode, orbis::Thread *thread) {
  auto realPath = hostPath + path;

  int realFlags = flags & O_ACCMODE;
  flags &= ~O_ACCMODE;

  if ((flags & kOpenFlagAppend) != 0) {
    realFlags |= O_APPEND;
    flags &= ~kOpenFlagAppend;
  }

  if ((flags & kOpenFlagNonBlock) != 0) {
    realFlags |= O_NONBLOCK;
    flags &= ~kOpenFlagNonBlock;
  }

  if ((flags & kOpenFlagFsync) != 0) {
    realFlags |= O_FSYNC;
    flags &= ~kOpenFlagFsync;
  }

  if ((flags & kOpenFlagAsync) != 0) {
    realFlags |= O_ASYNC;
    flags &= ~kOpenFlagAsync;
  }

  if ((flags & kOpenFlagTrunc) != 0) {
    realFlags |= O_TRUNC;
    flags &= ~kOpenFlagTrunc;
  }

  if ((flags & kOpenFlagCreat) != 0) {
    realFlags |= O_CREAT;
    flags &= ~kOpenFlagCreat;
  }

  if ((flags & kOpenFlagExcl) != 0) {
    realFlags |= O_EXCL;
    flags &= ~kOpenFlagExcl;
  }

  if ((flags & kOpenFlagDirectory) != 0) {
    realFlags |= O_DIRECTORY;
    flags &= ~kOpenFlagDirectory;
  }

  if (flags != 0) {
    ORBIS_LOG_ERROR("host_open: ***ERROR*** Unhandled open flags", flags);
  }

  int hostFd = ::open(realPath.c_str(), realFlags, 0777);

  if (hostFd < 0) {
    auto error = convertErrno();
    ORBIS_LOG_ERROR("host_open failed", realPath, error);
    return error;
  }

  auto newFile = orbis::knew<HostFile>();
  newFile->hostFd = hostFd;
  newFile->ops = &hostOps;
  newFile->device = this;
  *file = newFile;
  return {};
}

orbis::File *createHostFile(int hostFd, orbis::Ref<IoDevice> device) {
  auto newFile = orbis::knew<HostFile>();
  newFile->hostFd = hostFd;
  newFile->ops = &hostOps;
  newFile->device = device;
  return newFile;
}

struct FdWrapDevice : public IoDevice {
  int fd;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    *file = createHostFile(fd, this);
    return {};
  }
};

IoDevice *createFdWrapDevice(int fd) {
  auto result = orbis::knew<FdWrapDevice>();
  result->fd = fd;
  return result;
}
