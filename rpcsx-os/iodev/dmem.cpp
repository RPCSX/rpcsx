#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"
#include <cinttypes>
#include <cstdio>

struct DmemDevice : public IoDevice {
  int index;
  std::uint64_t nextOffset;
  std::uint64_t memBeginAddress;
};

struct DmemInstance : public IoDeviceInstance {};

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

static std::int64_t dmem_instance_ioctl(IoDeviceInstance *instance,
                                        std::uint64_t request, void *argp) {

  auto device = static_cast<DmemDevice *>(instance->device.get());
  switch (request) {
  case 0x4008800a: // get size
    ORBIS_LOG_ERROR("dmem getTotalSize", device->index, argp);
    *(std::uint64_t *)argp = dmemSize;
    return 0;

  case 0xc0208016: // get avaiable size
    ORBIS_LOG_ERROR("dmem getAvaiableSize", device->index, argp);
    *(std::uint64_t *)argp = dmemSize - device->nextOffset;
    return 0;

  case 0xc0288001: { // sceKernelAllocateDirectMemory
    auto args = reinterpret_cast<AllocateDirectMemoryArgs *>(argp);
    auto alignedOffset =
        (device->nextOffset + args->alignment - 1) & ~(args->alignment - 1);

    ORBIS_LOG_ERROR("dmem allocateDirectMemory", device->index,
                    args->searchStart, args->searchEnd, args->len,
                    args->alignment, args->memoryType, alignedOffset);

    if (alignedOffset + args->len > dmemSize) {
      return -1;
    }

    args->searchStart = alignedOffset;
    device->nextOffset = alignedOffset + args->len;
    return 0;
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
    return 0;
  }

  default:

    ORBIS_LOG_FATAL("Unhandled dmem ioctl", device->index, request);
    return 0;

    std::fflush(stdout);
    __builtin_trap();
  }

  return -1;
}

static void *dmem_instance_mmap(IoDeviceInstance *instance, void *address,
                                std::uint64_t size, std::int32_t prot,
                                std::int32_t flags, std::int64_t offset) {
  auto device = static_cast<DmemDevice *>(instance->device.get());
  auto target = device->memBeginAddress + offset;
  ORBIS_LOG_WARNING("dmem mmap", device->index, offset, target);

  auto addr = rx::vm::map(reinterpret_cast<void *>(target), size, prot, flags);
  return addr;
}

static std::int32_t dmem_device_open(IoDevice *device,
                                     orbis::Ref<IoDeviceInstance> *instance,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode) {
  auto *newInstance = orbis::knew<DmemInstance>();
  newInstance->ioctl = dmem_instance_ioctl;
  newInstance->mmap = dmem_instance_mmap;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createDmemCharacterDevice(int index) {
  auto *newDevice = orbis::knew<DmemDevice>();
  newDevice->open = dmem_device_open;
  newDevice->index = index;
  newDevice->nextOffset = 0;
  newDevice->memBeginAddress = 0xf'e000'0000 + dmemSize * index;
  return newDevice;
}
