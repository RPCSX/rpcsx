#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "vm.hpp"
#include <mutex>

struct DmemDevice : public IoDevice {
  orbis::shared_mutex mtx;
  int index;
  std::uint64_t nextOffset;
  std::uint64_t memBeginAddress;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

struct DmemFile : public orbis::File {};

struct AllocateDirectMemoryArgs {
  std::uint64_t searchStart;
  std::uint64_t searchEnd;
  std::uint64_t len;
  std::uint64_t alignment;
  std::uint32_t memoryType;
};

static constexpr auto dmemSize = 4ul * 1024 * 1024 * 1024;
// static const std::uint64_t nextOffset = 0;
//  static const std::uint64_t memBeginAddress = 0xfe0000000;

static orbis::ErrorCode dmem_ioctl(orbis::File *file, std::uint64_t request,
                                   void *argp, orbis::Thread *thread) {
  auto device = static_cast<DmemDevice *>(file->device.get());

  std::lock_guard lock(device->mtx);
  switch (request) {
  case 0x4008800a: // get size
    ORBIS_LOG_ERROR("dmem getTotalSize", device->index, argp);
    *(std::uint64_t *)argp = dmemSize;
    return {};

  case 0xc0208016: // get avaiable size
    ORBIS_LOG_ERROR("dmem getAvaiableSize", device->index, argp);
    *(std::uint64_t *)argp = dmemSize - device->nextOffset;
    return {};

  case 0xc0288001: { // sceKernelAllocateDirectMemory
    auto args = reinterpret_cast<AllocateDirectMemoryArgs *>(argp);
    auto alignedOffset =
        (device->nextOffset + args->alignment - 1) & ~(args->alignment - 1);

    ORBIS_LOG_ERROR("dmem allocateDirectMemory", device->index,
                    args->searchStart, args->searchEnd, args->len,
                    args->alignment, args->memoryType, alignedOffset);

    if (alignedOffset + args->len > dmemSize) {
      return orbis::ErrorCode::NOMEM;
    }

    args->searchStart = alignedOffset;
    device->nextOffset = alignedOffset + args->len;
    return {};
  }

  case 0x80108002: { // sceKernelReleaseDirectMemory
    struct Args {
      std::uint64_t address;
      std::uint64_t size;
    };

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_TODO("dmem releaseDirectMemory", device->index, args->address,
                   args->size);
    // std::fflush(stdout);
    //__builtin_trap();
    return {};
  }

  default:
    ORBIS_LOG_FATAL("Unhandled dmem ioctl", device->index, request);
    return {};
  }
}

static orbis::ErrorCode dmem_mmap(orbis::File *file, void **address,
                                  std::uint64_t size, std::int32_t prot,
                                  std::int32_t flags, std::int64_t offset,
                                  orbis::Thread *thread) {
  auto device = static_cast<DmemDevice *>(file->device.get());
  auto target = device->memBeginAddress + offset;
  ORBIS_LOG_WARNING("dmem mmap", device->index, offset, target);

  auto result =
      rx::vm::map(reinterpret_cast<void *>(target), size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = dmem_ioctl,
    .mmap = dmem_mmap,
};

orbis::ErrorCode DmemDevice::open(orbis::Ref<orbis::File> *file,
                                  const char *path, std::uint32_t flags,
                                  std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<DmemFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createDmemCharacterDevice(int index) {
  auto *newDevice = orbis::knew<DmemDevice>();
  newDevice->index = index;
  newDevice->nextOffset = 0;
  newDevice->memBeginAddress = 0xf'e000'0000 + dmemSize * index;
  return newDevice;
}
