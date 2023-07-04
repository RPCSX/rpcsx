#include <fstream>

#include "rpcsx-os/io-device.hpp"
#include "orbis/KernelAllocator.hpp"

struct StderrInstance : public IoDeviceInstance {};

struct StderrDevice : public IoDevice {
  StderrInstance *instance = nullptr;
};

static std::int64_t stderr_instance_write(IoDeviceInstance *instance, const void *data, std::uint64_t size) {
  auto result = fwrite(data, 1, size, stderr);
  fflush(stderr);

  return result;
}

static std::int64_t stderr_instance_close(IoDeviceInstance *instance) {
  instance->device->decRef();
  return 0;
}

static std::int32_t stderr_device_open(IoDevice *device,
                                       orbis::Ref<IoDeviceInstance> *instance,
                                       const char *path, std::uint32_t flags,
                                       std::uint32_t mode) {
  auto stderrDevice = static_cast<StderrDevice *>(device);
  if (stderrDevice->instance == nullptr) {
    auto *newInstance = orbis::knew<StderrInstance>();
    newInstance->write = stderr_instance_write;
    newInstance->close = stderr_instance_close;

    io_device_instance_init(device, newInstance);

    *instance = newInstance;
  } else {
    device->incRef();

    *instance = stderrDevice->instance;
  }

  return 0;
}

IoDevice *createStderrCharacterDevice() {
  auto *newDevice = orbis::knew<StderrDevice>();
  newDevice->open = stderr_device_open;
  return newDevice;
}
