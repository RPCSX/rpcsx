#include <cinttypes>
#include <cstdio>

#include "rpcsx-os/vm.hpp"
#include "rpcsx-os/io-device.hpp"
#include "orbis/KernelAllocator.hpp"

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
// static const std::uint64_t memBeginAddress = 0xfe0000000;

static std::int64_t dmem_instance_ioctl(IoDeviceInstance *instance,
                                        std::uint64_t request, void *argp) {

  auto device = static_cast<DmemDevice *>(instance->device.get());
  switch (request) {
  case 0x4008800a: // get size
    std::fprintf(stderr, "dmem%u getTotalSize(%p)\n", device->index, argp);
    *(std::uint64_t *)argp = dmemSize;
    return 0;

  case 0xc0208016: // get avaiable size
    std::fprintf(stderr, "dmem%u getAvaiableSize(%p)\n", device->index, argp);
    *(std::uint64_t *)argp = dmemSize - device->nextOffset;
    return 0;

  case 0xc0288001: { // sceKernelAllocateDirectMemory
    auto args = reinterpret_cast<AllocateDirectMemoryArgs *>(argp);
    auto alignedOffset =
      (device->nextOffset + args->alignment - 1) & ~(args->alignment - 1);

    std::fprintf(
      stderr,
      "dmem%u allocateDirectMemory(searchStart = %lx, searchEnd = %lx, len "
      "= %lx, alignment = %lx, memoryType = %x) -> 0x%lx\n",
      device->index, args->searchStart, args->searchEnd, args->len,
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

    std::fprintf(
      stderr, "TODO: dmem%u releaseDirectMemory(address=0x%lx, size=0x%lx)\n",
      device->index, args->address, args->size);
    //std::fflush(stdout);
    //__builtin_trap();
    return 0;
  }

  default:
    std::fprintf(stderr, "***ERROR*** Unhandled dmem%u ioctl %lx\n",
                 static_cast<DmemDevice *>(instance->device.get())->index, request);

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
  std::fprintf(stderr, "WARNING: dmem%u mmap %lx -> %lx\n", device->index,
               offset, device->memBeginAddress + offset);

  auto addr =
    rx::vm::map(reinterpret_cast<void *>(device->memBeginAddress + offset), size,
                prot, flags);
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
