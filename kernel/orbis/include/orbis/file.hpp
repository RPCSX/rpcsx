#pragma once

#include "IoDevice.hpp"
#include "KernelAllocator.hpp"
#include "error/ErrorCode.hpp"
#include "note.hpp"
#include "rx/Mappable.hpp"
#include "rx/Rc.hpp"
#include "rx/SharedMutex.hpp"
#include "stat.hpp"
#include <cstdint>

namespace orbis {
struct File;
struct KNote;
struct Thread;
struct Stat;
struct StatFs;
struct Uio;
struct SocketAddress;
struct msghdr;
struct sf_hdtr;

struct FileOps {
  std::int32_t flags;
  ErrorCode (*ioctl)(File *file, std::uint64_t request, void *argp,
                     Thread *thread) = nullptr;
  ErrorCode (*read)(File *file, Uio *uio, Thread *thread) = nullptr;
  ErrorCode (*write)(File *file, Uio *uio, Thread *thread) = nullptr;

  ErrorCode (*truncate)(File *file, std::uint64_t len,
                        Thread *thread) = nullptr;

  ErrorCode (*poll)(File *file, std::uint32_t events, Thread *thread) = nullptr;

  ErrorCode (*kqfilter)(File *file, KNote *kn, Thread *thread) = nullptr;

  ErrorCode (*stat)(File *file, Stat *sb, Thread *thread) = nullptr;
  ErrorCode (*statfs)(File *file, StatFs *sb, Thread *thread) = nullptr;

  ErrorCode (*mkdir)(File *file, const char *path, std::int32_t mode) = nullptr;

  // TODO: chown
  // TODO: chmod

  ErrorCode (*bind)(File *file, SocketAddress *address, std::size_t addressLen,
                    Thread *thread) = nullptr;
  ErrorCode (*listen)(File *file, int backlog, Thread *thread) = nullptr;
  ErrorCode (*accept)(File *file, SocketAddress *address,
                      std::uint32_t *addressLen, Thread *thread) = nullptr;
  ErrorCode (*connect)(File *file, SocketAddress *address,
                       std::uint32_t addressLen, Thread *thread) = nullptr;
  ErrorCode (*sendto)(File *file, const void *buf, size_t len, sint flags,
                      caddr_t to, sint tolen, Thread *thread) = nullptr;
  ErrorCode (*sendmsg)(File *file, msghdr *msg, sint flags,
                       Thread *thread) = nullptr;
  ErrorCode (*recvfrom)(File *file, void *buf, size_t len, sint flags,
                        SocketAddress *from, uint32_t *fromlenaddr,
                        Thread *thread) = nullptr;
  ErrorCode (*recvmsg)(File *file, msghdr *msg, sint flags,
                       Thread *thread) = nullptr;
  ErrorCode (*shutdown)(File *file, sint how, Thread *thread) = nullptr;
  ErrorCode (*setsockopt)(File *file, sint level, sint name, const void *val,
                          sint valsize, Thread *thread) = nullptr;
  ErrorCode (*getsockopt)(File *file, sint level, sint name, void *val,
                          sint *avalsize, Thread *thread) = nullptr;
  ErrorCode (*sendfile)(File *file, sint fd, off_t offset, size_t nbytes,
                        ptr<struct sf_hdtr> hdtr, ptr<off_t> sbytes, sint flags,
                        Thread *thread) = nullptr;

  std::string (*toString)(File *file, Thread *thread);
};

struct File : rx::RcBase {
  rx::shared_mutex mtx;
  rx::Ref<EventEmitter> event;
  const FileOps *ops = nullptr;
  rx::Ref<IoDevice> device;
  std::uint64_t nextOff = 0;
  int flags = 0;
  int mode = 0;
  rx::Mappable hostFd;
  kvector<Dirent> dirEntries;

  bool noBlock() const { return (flags & 4) != 0; }
};
} // namespace orbis
