#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/stat.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
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
  bool closeOnExit = true;

  ~HostFile() {
    if (hostFd > 0 && closeOnExit) {
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
  orbis::ErrorCode unlink(const char *path, bool recursive,
                          orbis::Thread *thread) override;
  orbis::ErrorCode createSymlink(const char *target, const char *linkPath,
                                 orbis::Thread *thread) override;
  orbis::ErrorCode mkdir(const char *path, int mode,
                         orbis::Thread *thread) override;
  orbis::ErrorCode rmdir(const char *path, orbis::Thread *thread) override;
  orbis::ErrorCode rename(const char *from, const char *to,
                          orbis::Thread *thread) override;
};

static orbis::ErrorCode convertErrc(std::errc errc) {
  if (errc == std::errc{}) {
    return {};
  }

  switch (errc) {
  case std::errc::address_family_not_supported:
    return orbis::ErrorCode::AFNOSUPPORT;
  case std::errc::address_in_use:
    return orbis::ErrorCode::ADDRINUSE;
  case std::errc::address_not_available:
    return orbis::ErrorCode::ADDRNOTAVAIL;
  case std::errc::already_connected:
    return orbis::ErrorCode::ISCONN;
  case std::errc::argument_out_of_domain:
    return orbis::ErrorCode::DOM;
  case std::errc::bad_address:
    return orbis::ErrorCode::FAULT;
  case std::errc::bad_file_descriptor:
    return orbis::ErrorCode::BADF;
  case std::errc::bad_message:
    return orbis::ErrorCode::BADMSG;
  case std::errc::broken_pipe:
    return orbis::ErrorCode::PIPE;
  case std::errc::connection_aborted:
    return orbis::ErrorCode::CONNABORTED;
  case std::errc::connection_already_in_progress:
    return orbis::ErrorCode::ALREADY;
  case std::errc::connection_refused:
    return orbis::ErrorCode::CONNREFUSED;
  case std::errc::connection_reset:
    return orbis::ErrorCode::CONNRESET;
  case std::errc::cross_device_link:
    return orbis::ErrorCode::XDEV;
  case std::errc::destination_address_required:
    return orbis::ErrorCode::DESTADDRREQ;
  case std::errc::device_or_resource_busy:
    return orbis::ErrorCode::BUSY;
  case std::errc::directory_not_empty:
    return orbis::ErrorCode::NOTEMPTY;
  case std::errc::executable_format_error:
    return orbis::ErrorCode::NOEXEC;
  case std::errc::file_exists:
    return orbis::ErrorCode::EXIST;
  case std::errc::file_too_large:
    return orbis::ErrorCode::FBIG;
  case std::errc::filename_too_long:
    return orbis::ErrorCode::NAMETOOLONG;
  case std::errc::function_not_supported:
    return orbis::ErrorCode::NOSYS;
  case std::errc::host_unreachable:
    return orbis::ErrorCode::HOSTUNREACH;
  case std::errc::identifier_removed:
    return orbis::ErrorCode::IDRM;
  case std::errc::illegal_byte_sequence:
    return orbis::ErrorCode::ILSEQ;
  case std::errc::inappropriate_io_control_operation:
    return orbis::ErrorCode::NOTTY;
  case std::errc::interrupted:
    return orbis::ErrorCode::INTR;
  case std::errc::invalid_argument:
    return orbis::ErrorCode::INVAL;
  case std::errc::invalid_seek:
    return orbis::ErrorCode::SPIPE;
  case std::errc::io_error:
    return orbis::ErrorCode::IO;
  case std::errc::is_a_directory:
    return orbis::ErrorCode::ISDIR;
  case std::errc::message_size:
    return orbis::ErrorCode::MSGSIZE;
  case std::errc::network_down:
    return orbis::ErrorCode::NETDOWN;
  case std::errc::network_reset:
    return orbis::ErrorCode::NETRESET;
  case std::errc::network_unreachable:
    return orbis::ErrorCode::NETUNREACH;
  case std::errc::no_buffer_space:
    return orbis::ErrorCode::NOBUFS;
  case std::errc::no_child_process:
    return orbis::ErrorCode::CHILD;
  case std::errc::no_link:
    return orbis::ErrorCode::NOLINK;
  case std::errc::no_lock_available:
    return orbis::ErrorCode::NOLCK;
  case std::errc::no_message:
    return orbis::ErrorCode::NOMSG;
  case std::errc::no_protocol_option:
    return orbis::ErrorCode::NOPROTOOPT;
  case std::errc::no_space_on_device:
    return orbis::ErrorCode::NOSPC;
  case std::errc::no_such_device_or_address:
    return orbis::ErrorCode::NXIO;
  case std::errc::no_such_device:
    return orbis::ErrorCode::NODEV;
  case std::errc::no_such_file_or_directory:
    return orbis::ErrorCode::NOENT;
  case std::errc::no_such_process:
    return orbis::ErrorCode::SRCH;
  case std::errc::not_a_directory:
    return orbis::ErrorCode::NOTDIR;
  case std::errc::not_a_socket:
    return orbis::ErrorCode::NOTSOCK;
  case std::errc::not_connected:
    return orbis::ErrorCode::NOTCONN;
  case std::errc::not_enough_memory:
    return orbis::ErrorCode::NOMEM;
  case std::errc::not_supported:
    return orbis::ErrorCode::NOTSUP;
  case std::errc::operation_canceled:
    return orbis::ErrorCode::CANCELED;
  case std::errc::operation_in_progress:
    return orbis::ErrorCode::INPROGRESS;
  case std::errc::operation_not_permitted:
    return orbis::ErrorCode::PERM;
  case std::errc::operation_would_block:
    return orbis::ErrorCode::WOULDBLOCK;
  case std::errc::permission_denied:
    return orbis::ErrorCode::ACCES;
  case std::errc::protocol_error:
    return orbis::ErrorCode::PROTO;
  case std::errc::protocol_not_supported:
    return orbis::ErrorCode::PROTONOSUPPORT;
  case std::errc::read_only_file_system:
    return orbis::ErrorCode::ROFS;
  case std::errc::resource_deadlock_would_occur:
    return orbis::ErrorCode::DEADLK;
  case std::errc::result_out_of_range:
    return orbis::ErrorCode::RANGE;
  case std::errc::text_file_busy:
    return orbis::ErrorCode::TXTBSY;
  case std::errc::timed_out:
    return orbis::ErrorCode::TIMEDOUT;
  case std::errc::too_many_files_open_in_system:
    return orbis::ErrorCode::NFILE;
  case std::errc::too_many_files_open:
    return orbis::ErrorCode::MFILE;
  case std::errc::too_many_links:
    return orbis::ErrorCode::MLINK;
  case std::errc::too_many_symbolic_link_levels:
    return orbis::ErrorCode::LOOP;
  case std::errc::value_too_large:
    return orbis::ErrorCode::OVERFLOW;
  case std::errc::wrong_protocol_type:
    return orbis::ErrorCode::PROTOTYPE;
  default:
    return orbis::ErrorCode::FAULT;
  }
}

static orbis::ErrorCode convertErrorCode(const std::error_code &code) {
  if (!code) {
    return {};
  }
  return convertErrc(static_cast<std::errc>(code.value()));
}

orbis::ErrorCode convertErrno() {
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
  if (!hostFile->dirEntries.empty())
    return orbis::ErrorCode::ISDIR;

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
  if (!hostFile->dirEntries.empty())
    return orbis::ErrorCode::ISDIR;

  std::vector<iovec> vec;
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    vec.push_back({.iov_base = entry.base, .iov_len = entry.len});
  }

  ssize_t cnt =
      ::pwritev(hostFile->hostFd, vec.data(), vec.size(), uio->offset);

  if (cnt < 0) {
    for (auto io : vec) {
      auto result = ::write(hostFile->hostFd, io.iov_base, io.iov_len);
      if (result < 0) {
        return convertErrno();
      }

      cnt += result;

      if (cnt != io.iov_len) {
        break;
      }
    }
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
  if (!hostFile->dirEntries.empty())
    return orbis::ErrorCode::ISDIR;

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
  if (!hostFile->dirEntries.empty())
    return orbis::ErrorCode::ISDIR;

  // hack for audio control shared memory
  std::uint64_t realLen = len;
  if (len == 3880) {
    realLen = 0x10000;
  }
  if (::ftruncate(hostFile->hostFd, realLen)) {
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
  while (hostPath.size() > 1 && hostPath.ends_with("/")) {
    hostPath.resize(hostPath.size() - 1);
  }

  return orbis::knew<HostFsDevice>(std::move(hostPath));
}

orbis::ErrorCode createSocket(orbis::Ref<orbis::File> *file,
                              orbis::kstring name, int dom, int type,
                              int prot) {
  auto fd = ::socket(dom, type, prot);
  // if (fd < 0) {
  //   return convertErrno();
  // }

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
  auto realPath = hostPath + "/" + path;

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
    ORBIS_LOG_ERROR("host_open failed", path, realPath, error);
    return error;
  }

  // Assume the file is a directory and try to read direntries
  orbis::utils::kvector<orbis::Dirent> dirEntries;
  while (true) {
    ::dirent64 hostEntry{};
    auto r = getdents64(hostFd, &hostEntry, sizeof(hostEntry));
    if (r <= 0)
      break;

    if (hostEntry.d_name == std::string_view("..") ||
        hostEntry.d_name == std::string_view(".")) {
      continue;
    }

    auto &entry = dirEntries.emplace_back();
    entry.fileno = dirEntries.size(); // TODO
    entry.reclen = sizeof(entry);
    entry.namlen = std::strlen(hostEntry.d_name);
    std::strncpy(entry.name, hostEntry.d_name, sizeof(entry.name));
    if (hostEntry.d_type == DT_REG)
      entry.type = orbis::kDtReg;
    else if (hostEntry.d_type == DT_DIR || hostEntry.d_type == DT_LNK)
      entry.type = orbis::kDtDir; // Assume symlinks to be dirs
    else {
      ORBIS_LOG_ERROR("host_open: unknown directory entry d_type", realPath,
                      hostEntry.d_name, hostEntry.d_type);
      dirEntries.pop_back();
    }
  }

  auto newFile = orbis::knew<HostFile>();
  newFile->hostFd = hostFd;
  newFile->dirEntries = std::move(dirEntries);
  newFile->ops = &hostOps;
  newFile->device = this;
  *file = newFile;
  return {};
}

orbis::ErrorCode HostFsDevice::unlink(const char *path, bool recursive,
                                      orbis::Thread *thread) {
  std::error_code ec;

  if (recursive) {
    std::filesystem::remove_all(hostPath + "/" + path, ec);
  } else {
    std::filesystem::remove(hostPath + "/" + path, ec);
  }

  return convertErrorCode(ec);
}

orbis::ErrorCode HostFsDevice::createSymlink(const char *linkPath,
                                             const char *target,
                                             orbis::Thread *thread) {
  std::error_code ec;
  std::filesystem::create_symlink(
      std::filesystem::absolute(hostPath + "/" + linkPath),
      hostPath + "/" + target, ec);
  return convertErrorCode(ec);
}

orbis::ErrorCode HostFsDevice::mkdir(const char *path, int mode,
                                     orbis::Thread *thread) {
  std::error_code ec;
  std::filesystem::create_directories(hostPath + "/" + path, ec);
  return convertErrorCode(ec);
}
orbis::ErrorCode HostFsDevice::rmdir(const char *path, orbis::Thread *thread) {
  std::error_code ec;
  std::filesystem::remove_all(hostPath + "/" + path, ec);
  return convertErrorCode(ec);
}
orbis::ErrorCode HostFsDevice::rename(const char *from, const char *to,
                                      orbis::Thread *thread) {
  std::error_code ec;
  std::filesystem::rename(hostPath + "/" + from, hostPath + "/" + to, ec);
  return convertErrorCode(ec);
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
    static_cast<HostFile *>(file->get())->closeOnExit = false;
    return {};
  }
};

IoDevice *createFdWrapDevice(int fd) {
  auto result = orbis::knew<FdWrapDevice>();
  result->fd = fd;
  return result;
}
