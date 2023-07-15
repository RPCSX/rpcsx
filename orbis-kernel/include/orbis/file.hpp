#pragma once

#include "KernelAllocator.hpp"
#include "error/ErrorCode.hpp"
#include <atomic>
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
  ErrorCode (*rdwr)(File *file, Uio *uio, Thread *thread) = nullptr;
  ErrorCode (*close)(File *file, Thread *thread) = nullptr;
  ErrorCode (*lseek)(File *file, std::uint64_t *offset, std::uint32_t whence,
                     Thread *thread) = nullptr;

  ErrorCode (*truncate)(File *file, std::uint64_t len,
                        Thread *thread) = nullptr;

  ErrorCode (*poll)(File *file, std::uint32_t events, Thread *thread) = nullptr;

  ErrorCode (*kqfilter)(File *file, KNote *kn, Thread *thread) = nullptr;

  ErrorCode (*stat)(File *file, Stat *sb, Thread *thread) = nullptr;

  // TODO: chown
  // TODO: chmod

  ErrorCode (*mmap)(File *file, void **address, std::uint64_t size,
                    std::int32_t prot, std::int32_t flags, std::int64_t offset,
                    Thread *thread) = nullptr;
  ErrorCode (*munmap)(File *file, void **address, std::uint64_t size,
                      Thread *thread) = nullptr;
};

struct File final {
  FileOps *ops;
  Ref<RcBase> device;
  std::atomic<unsigned> refs{0};

  void incRef() { refs.fetch_add(1, std::memory_order::acquire); }

  void decRef() {
    if (refs.fetch_sub(1, std::memory_order::release) == 1) {
      kdelete(this);
    }
  }
};
} // namespace orbis
