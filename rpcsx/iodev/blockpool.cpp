#include "blockpool.hpp"
#include "dmem.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"
#include <mutex>
#include <sys/mman.h>

struct BlockPoolFile : public orbis::File {};

static orbis::ErrorCode blockpool_ioctl(orbis::File *file,
                                        std::uint64_t request, void *argp,
                                        orbis::Thread *thread) {
  auto blockPool = static_cast<BlockPoolDevice *>(file->device.get());
  std::lock_guard lock(blockPool->mtx);

  switch (request) {
  case 0xc020a801: {
    struct Args {
      std::uint64_t len;
      std::uint64_t searchStart;
      std::uint64_t searchEnd;
      std::uint32_t flags;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_TODO("blockpool expand", args->len, args->searchStart,
                   args->searchEnd, args->flags);

    // auto dmem = static_cast<DmemDevice *>(orbis::g_context.dmemDevice.get());
    // std::lock_guard lock(dmem->mtx);
    // std::uint64_t start = args->searchStart;
    // std::uint64_t len = std::min(args->searchEnd - start, args->len);
    // if (dmem->nextOffset > args->searchEnd) {
    //   ORBIS_LOG_TODO("blockpool out of allocation", args->len,
    //                  args->searchStart, args->searchEnd, args->flags);
    //   return orbis::ErrorCode::INVAL;
    // }

    // start = std::max(dmem->nextOffset, start);
    // auto end = std::min(start + len, args->searchEnd);
    // dmem->nextOffset = end;
    // args->searchStart = start;

    // blockPool->len += end - start;
    return {};
  }
  }

  ORBIS_LOG_FATAL("Unhandled blockpool ioctl", request);
  thread->where();
  return {};
}

static orbis::ErrorCode blockpool_mmap(orbis::File *file, void **address,
                                       std::uint64_t size, std::int32_t prot,
                                       std::int32_t flags, std::int64_t offset,
                                       orbis::Thread *thread) {
  auto blockPool = static_cast<BlockPoolDevice *>(file->device.get());
  std::lock_guard lock(blockPool->mtx);
  ORBIS_LOG_FATAL("blockpool mmap", *address, size, offset, blockPool->len);
  size = std::min<std::uint64_t>(
      0x1000000, size); // FIXME: hack, investigate why we report so many memory
  size = std::min(blockPool->len, size);
  auto result = vm::map(*address, size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  blockPool->len -= size;
  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = blockpool_ioctl,
    .mmap = blockpool_mmap,
};

orbis::ErrorCode BlockPoolDevice::open(orbis::Ref<orbis::File> *file,
                                       const char *path, std::uint32_t flags,
                                       std::uint32_t mode,
                                       orbis::Thread *thread) {
  auto newFile = orbis::knew<BlockPoolFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::ErrorCode BlockPoolDevice::map(void **address, std::uint64_t len,
                                      std::int32_t prot, std::int32_t flags,
                                      orbis::Thread *thread) {
  ORBIS_LOG_FATAL("blockpool device map", *address, len);
  auto result = vm::map(*address, len, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::NOMEM;
  }

  *address = result;
  return {};
}
orbis::ErrorCode BlockPoolDevice::unmap(void *address, std::uint64_t len,
                                        orbis::Thread *thread) {
  ORBIS_LOG_FATAL("blockpool device unmap", address, len);
  if (vm::unmap(address, len)) {
    return {};
  }
  return orbis::ErrorCode::INVAL;
}
IoDevice *createBlockPoolDevice() { return orbis::knew<BlockPoolDevice>(); }
