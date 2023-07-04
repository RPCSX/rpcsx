#include <cstring>

#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"

struct ZeroDevice : public IoDevice {
};

struct ZeroInstance : public IoDeviceInstance {
};

static std::int64_t zero_device_read(IoDeviceInstance *instance, void *data,
                                     std::uint64_t size) {
  std::memset(data, 0, size);
  return size;
}

static std::int32_t zero_device_open(IoDevice *device,
                                     orbis::Ref<IoDeviceInstance> *instance,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode) {
  auto *newInstance = orbis::knew<ZeroInstance>();
  newInstance->read = zero_device_read;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createZeroCharacterDevice() {
  auto *newDevice = orbis::knew<ZeroDevice>();
  newDevice->open = zero_device_open;
  return newDevice;
}
