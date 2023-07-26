#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"
#include <cstdio>

struct AjmDevice : public IoDevice {};

struct AjmInstance : public IoDeviceInstance {};

static std::int64_t ajm_instance_ioctl(IoDeviceInstance *instance,
                                       std::uint64_t request, void *argp) {

  ORBIS_LOG_FATAL("Unhandled AJM ioctl", request);
  return -1;
}

static std::int32_t ajm_device_open(IoDevice *device,
                                    orbis::Ref<IoDeviceInstance> *instance,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode) {
  auto *newInstance = orbis::knew<AjmInstance>();
  newInstance->ioctl = ajm_instance_ioctl;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createAjmCharacterDevice() {
  auto *newDevice = orbis::knew<AjmDevice>();
  newDevice->open = ajm_device_open;
  return newDevice;
}