#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"
#include <cstdio>

struct Hmd3daDevice : public IoDevice {};

struct Hmd3daInstance : public IoDeviceInstance {};

static std::int64_t hmd_3da_instance_ioctl(IoDeviceInstance *instance,
                                           std::uint64_t request, void *argp) {

  ORBIS_LOG_FATAL("Unhandled hmd_3da ioctl", request);
  return -1;
}

static std::int32_t hmd_3da_device_open(IoDevice *device,
                                        orbis::Ref<IoDeviceInstance> *instance,
                                        const char *path, std::uint32_t flags,
                                        std::uint32_t mode) {
  auto *newInstance = orbis::knew<Hmd3daInstance>();
  newInstance->ioctl = hmd_3da_instance_ioctl;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createHmd3daCharacterDevice() {
  auto *newDevice = orbis::knew<Hmd3daDevice>();
  newDevice->open = hmd_3da_device_open;
  return newDevice;
}
