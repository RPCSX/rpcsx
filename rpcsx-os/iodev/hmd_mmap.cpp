#include <cinttypes>
#include <cstdio>

#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "vm.hpp"

struct HmdMmapDevice : public IoDevice {};

struct HmdMmapInstance : public IoDeviceInstance {
};

static std::int64_t hmd_mmap_instance_ioctl(IoDeviceInstance *instance,
                                            std::uint64_t request, void *argp) {

  std::fprintf(stderr, "***ERROR*** Unhandled hmd_mmap ioctl %lx\n",
               request);
  std::fflush(stdout);
  __builtin_trap();
  return -1;
}

static void *hmd_mmap_instance_mmap(IoDeviceInstance *instance,
                                    void *address, std::uint64_t size,
                                    std::int32_t prot,
                                    std::int32_t flags,
                                    std::int64_t offset) {
  std::fprintf(stderr, "***ERROR*** Unhandled hmd_mmap mmap %lx\n", offset);
  return rx::vm::map(address, size, prot, flags);
}

static std::int32_t hmd_mmap_device_open(IoDevice *device,
                                         orbis::Ref<IoDeviceInstance> *instance,
                                         const char *path, std::uint32_t flags,
                                         std::uint32_t mode) {
  auto *newInstance = orbis::knew<HmdMmapInstance>();
  newInstance->ioctl = hmd_mmap_instance_ioctl;
  newInstance->mmap = hmd_mmap_instance_mmap;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createHmdMmapCharacterDevice() {
  auto *newDevice = orbis::knew<HmdMmapDevice>();
  newDevice->open = hmd_mmap_device_open;
  return newDevice;
}
