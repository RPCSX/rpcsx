#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"
#include <cstdio>

struct DipswDevice : public IoDevice {};

struct DipswInstance : public IoDeviceInstance {};

static std::int64_t dipsw_instance_ioctl(IoDeviceInstance *instance,
                                         std::uint64_t request, void *argp) {
  if (request == 0x40048806) { // is connected?
    ORBIS_LOG_ERROR("dipsw ioctl 0x40048806", argp);

    *reinterpret_cast<std::uint32_t *>(argp) = 0;
    return 0;
  }

  // 0x40088808
  // 0x40088809

  if (request == 0x40088808) {
    ORBIS_LOG_ERROR("dipsw ioctl 0x40088808", argp);
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

    ORBIS_LOG_ERROR("dipsw ioctl 0x8010880a", args->address, args->size);

    return 0;
  }

  ORBIS_LOG_FATAL("Unhandled dipsw ioctl", request);
  std::fflush(stdout);
  //__builtin_trap();
  return 0;
}

static std::int32_t dipsw_device_open(IoDevice *device,
                                      orbis::Ref<IoDeviceInstance> *instance,
                                      const char *path, std::uint32_t flags,
                                      std::uint32_t mode) {
  auto *newInstance = orbis::knew<DipswInstance>();
  newInstance->ioctl = dipsw_instance_ioctl;
  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createDipswCharacterDevice() {
  auto *newDevice = orbis::knew<DipswDevice>();
  newDevice->open = dipsw_device_open;
  return newDevice;
}
