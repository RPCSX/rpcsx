#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include <cstdio>

struct StdoutInstance : public IoDeviceInstance {};

struct StdoutDevice : public IoDevice {
  StdoutInstance *instance = nullptr;
};

static std::int64_t stdout_instance_write(IoDeviceInstance *instance,
                                          const void *data,
                                          std::uint64_t size) {
  static const bool istty = isatty(fileno(stdout));
  if (size && istty)
    std::fprintf(stdout, "\e[30;1m");
  auto result = std::fwrite(data, 1, size, stdout);
  if (size && istty)
    std::fprintf(stdout, "\e[0m");
  std::fflush(stdout);

  return result;
}

static std::int64_t stdout_instance_close(IoDeviceInstance *instance) {
  instance->device->decRef();
  return 0;
}

static std::int32_t stdout_device_open(IoDevice *device,
                                       orbis::Ref<IoDeviceInstance> *instance,
                                       const char *path, std::uint32_t flags,
                                       std::uint32_t mode) {
  auto stdoutDevice = static_cast<StdoutDevice *>(device);
  if (stdoutDevice->instance == nullptr) {
    auto *newInstance = orbis::knew<StdoutInstance>();
    newInstance->write = stdout_instance_write;
    newInstance->close = stdout_instance_close;
    io_device_instance_init(device, newInstance);

    *instance = newInstance;
  } else {
    device->incRef();
    *instance = stdoutDevice->instance;
  }

  return 0;
}

IoDevice *createStdoutCharacterDevice() {
  auto *newDevice = orbis::knew<StdoutDevice>();
  newDevice->open = stdout_device_open;
  return newDevice;
}
