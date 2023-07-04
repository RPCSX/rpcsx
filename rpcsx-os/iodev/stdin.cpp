#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"

struct StdinDevice : public IoDevice {
};

struct StdinInstance : public IoDeviceInstance {
};

static std::int64_t stdin_instance_read(IoDeviceInstance *instance, void *data,
                         std::uint64_t size) {
  return -1;
}

static std::int32_t open(IoDevice *device, orbis::Ref<IoDeviceInstance> *instance,
                         const char *path, std::uint32_t flags,
                         std::uint32_t mode) {
  auto *newInstance = orbis::knew<StdinInstance>();
  newInstance->read = stdin_instance_read;
  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createStdinCharacterDevice() {
  auto *newDevice = orbis::knew<StdinDevice>();
  newDevice->open = open;
  return newDevice;
}
