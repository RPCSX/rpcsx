#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include <cstdio>

struct HmdSnsrDevice : public IoDevice {};

struct HmdSnsrInstance : public IoDeviceInstance {
};

static std::int64_t smd_snr_instance_ioctl(IoDeviceInstance *instance,
                                           std::uint64_t request, void *argp) {

  std::fprintf(stderr, "***ERROR*** Unhandled hmd_snsr ioctl %lx\n",
        request);
  return -1;
}

static std::int32_t smd_snr_device_open(IoDevice *device,
                                        orbis::Ref<IoDeviceInstance>  *instance,
                                        const char *path, std::uint32_t flags,
                                        std::uint32_t mode) {
  auto *newInstance = orbis::knew<HmdSnsrInstance>();
  newInstance->ioctl = smd_snr_instance_ioctl;
  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createHmdSnsrCharacterDevice() {
  auto *newDevice = orbis::knew<HmdSnsrDevice>();
  newDevice->open = smd_snr_device_open;
  return newDevice;
}
