#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"

struct NullDevice : public IoDevice {};

struct NullInstance : public IoDeviceInstance {
};

static std::int64_t null_instance_write(IoDeviceInstance *instance,
                                        const void *data, std::uint64_t size) {
  return size;
}

static std::int32_t null_device_open(IoDevice *device, orbis::Ref<IoDeviceInstance> *instance,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode) {
  auto *newInstance = orbis::knew<NullInstance>();
  newInstance->write = null_instance_write;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createNullCharacterDevice() {
  auto *newDevice = orbis::knew<NullDevice>();
  newDevice->open = null_device_open;
  return newDevice;
}
