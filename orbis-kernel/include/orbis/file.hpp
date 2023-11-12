#pragma once

#include "KernelAllocator.hpp"
#include "error/ErrorCode.hpp"
#include "stat.hpp"
#include "utils/Rc.hpp"
#include "utils/SharedMutex.hpp"
#include <cstdint>

namespace orbis {
struct File;
struct KNote;
struct Thread;
struct Stat;
struct Uio;

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

  ErrorCode (*mkdir)(File *file, const char *path, std::int32_t mode) = nullptr;

  // TODO: chown
  // TODO: chmod

  ErrorCode (*mmap)(File *file, void **address, std::uint64_t size,
                    std::int32_t prot, std::int32_t flags, std::int64_t offset,
                    Thread *thread) = nullptr;
  ErrorCode (*munmap)(File *file, void **address, std::uint64_t size,
                      Thread *thread) = nullptr;
};

struct File : RcBase {
  shared_mutex mtx;
  const FileOps *ops = nullptr;
  Ref<RcBase> device;
  std::uint64_t nextOff = 0;
  utils::kvector<Dirent> dirEntries;
};
} // namespace orbis
