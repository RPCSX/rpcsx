#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "vm.hpp"
#include <cinttypes>
#include <cstdio>

struct RngDevice : public IoDevice {};
struct RngInstance : public IoDeviceInstance {};

static std::int64_t rng_instance_ioctl(IoDeviceInstance *instance,
                                       std::uint64_t request, void *argp) {
  std::fprintf(stderr, "***ERROR*** Unhandled rng ioctl %" PRIx64 "\n", request);
  return 0;
}

static void *rng_instance_mmap(IoDeviceInstance *instance, void *address,
                               std::uint64_t size, std::int32_t prot,
                               std::int32_t flags, std::int64_t offset) {
  std::fprintf(stderr, "***ERROR*** Unhandled rng mmap %" PRIx64 "\n", offset);
  return rx::vm::map(address, size, prot, flags);
}

static std::int32_t rng_device_open(IoDevice *device,
                                    orbis::Ref<IoDeviceInstance> *instance,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode) {
  auto *newInstance = orbis::knew<RngInstance>();
  newInstance->ioctl = rng_instance_ioctl;
  newInstance->mmap = rng_instance_mmap;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createRngCharacterDevice() {
  auto *newDevice = orbis::knew<RngDevice>();
  newDevice->open = rng_device_open;
  return newDevice;
}
