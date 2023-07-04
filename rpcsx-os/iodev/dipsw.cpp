#include <cstdio>

#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"

struct DmemDevice : public IoDevice {
};

struct DmemInstance : public IoDeviceInstance {
};

static std::int64_t dipsw_instance_ioctl(IoDeviceInstance *instance,
                                         std::uint64_t request, void *argp) {
  if (request == 0x40048806) { // is connected?
    std::fprintf(stderr, "dipsw ioctl 0x40048806(%p)\n", argp);

    *reinterpret_cast<std::uint32_t *>(argp) = 0;
    return 0;
  }

  // 0x40088808
  // 0x40088809

  if (request == 0x40088808) {
    std::fprintf(stderr, "dipsw ioctl 0x40088808(%p)\n", argp);
    *reinterpret_cast<std::uint32_t *>(argp) = 1;
    return 0;
  }


  // 0x8010880a
  if (request == 0x8010880a) { // write data? used on initilization
    struct Args {
      std::uint64_t address;
      std::uint64_t size;
    };

    auto args = reinterpret_cast<Args *>(argp);

    std::fprintf(stderr, "dipsw ioctl 0x8010880a(0x%lx, 0x%lx)\n", args->address, args->size);

    return 0;
  }

  std::fprintf(stderr, "***ERROR*** Unhandled dipsw ioctl %lx\n", request);
  std::fflush(stdout);
  //__builtin_trap();
  return 0;
}

static std::int32_t dipsw_device_open(IoDevice *device,
                                      orbis::Ref<IoDeviceInstance> *instance,
                                      const char *path, std::uint32_t flags,
                                      std::uint32_t mode) {
  auto *newInstance = orbis::knew<DmemInstance>();
  newInstance->ioctl = dipsw_instance_ioctl;
  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createDipswCharacterDevice() {
  auto *newDevice = orbis::knew<DmemDevice>();
  newDevice->open = dipsw_device_open;
  return newDevice;
}
