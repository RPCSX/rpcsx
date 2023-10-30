#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct MBusFile : orbis::File {};
struct MBusDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode mbus_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled mbus ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = mbus_ioctl,
};

orbis::ErrorCode MBusDevice::open(orbis::Ref<orbis::File> *file, const char *path,
                      std::uint32_t flags, std::uint32_t mode,
                      orbis::Thread *thread) {
  auto newFile = orbis::knew<MBusFile>();
  newFile->ops = &fileOps;
  newFile->device = this;

  *file = newFile;
  return {};
}

IoDevice *createMBusCharacterDevice() { return orbis::knew<MBusDevice>(); }
