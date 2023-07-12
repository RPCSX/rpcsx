#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"
#include <cstdio>

struct HmdCmdDevice : public IoDevice {};

struct HmdCmdInstance : public IoDeviceInstance {};

static std::int64_t hmd_cmd_instance_ioctl(IoDeviceInstance *instance,
                                           std::uint64_t request, void *argp) {

  ORBIS_LOG_FATAL("Unhandled hmd_cmd ioctl", request);
  return -1;
}

static std::int32_t hmd_cmd_device_open(IoDevice *device,
                                        orbis::Ref<IoDeviceInstance> *instance,
                                        const char *path, std::uint32_t flags,
                                        std::uint32_t mode) {
  auto *newInstance = orbis::knew<HmdCmdInstance>();
  newInstance->ioctl = hmd_cmd_instance_ioctl;

  io_device_instance_init(device, newInstance);

  *instance = newInstance;
  return 0;
}

IoDevice *createHmdCmdCharacterDevice() {
  auto *newDevice = orbis::knew<HmdCmdDevice>();
  newDevice->open = hmd_cmd_device_open;
  return newDevice;
}
