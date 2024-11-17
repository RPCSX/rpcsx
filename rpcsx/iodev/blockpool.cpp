#include "blockpool.hpp"
#include "dmem.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"
#include <cstddef>
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

    auto dmem = orbis::g_context.dmemDevice.rawStaticCast<DmemDevice>();
    std::lock_guard lock(dmem->mtx);
    std::uint64_t start = args->searchStart;
    ORBIS_RET_ON_ERROR(dmem->allocate(&start, args->searchEnd, args->len, 1, args->flags));

    blockPool->pool.map(start, start + args->len);
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
  ORBIS_LOG_FATAL("blockpool mmap", *address, size, offset);

  std::size_t totalBlockPoolSize = 0;
  for (auto entry : blockPool->pool) {
    totalBlockPoolSize += entry.endAddress - entry.beginAddress;
  }

  if (totalBlockPoolSize < size) {
    return orbis::ErrorCode::NOMEM;
  }

  auto dmem = orbis::g_context.dmemDevice.rawStaticCast<DmemDevice>();
  auto mapped = reinterpret_cast<std::byte *>(vm::map(*address, size, prot, flags, vm::kMapInternalReserveOnly, blockPool));

  if (mapped == MAP_FAILED) {
    return orbis::ErrorCode::NOMEM;
  }
  
  auto result = mapped;

  flags |= vm::kMapFlagFixed;
  flags &= ~vm::kMapFlagNoOverwrite;
  while (true) {
    auto entry = *blockPool->pool.begin();
    auto blockSize = std::min(entry.endAddress - entry.beginAddress, size);
    void *mapAddress = mapped;
    ORBIS_LOG_FATAL("blockpool mmap", mapAddress, blockSize, entry.beginAddress, blockSize);
    ORBIS_RET_ON_ERROR(dmem->mmap(&mapAddress, blockSize, prot, flags, entry.beginAddress));

    mapped += blockSize;
    size -= blockSize;
    blockPool->pool.unmap(entry.beginAddress, entry.beginAddress + blockSize);
    if (size == 0) {
      break;
    }
  }

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
  if (prot == 0) {
    // FIXME: investigate it
    prot = 0x33;
  }

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
