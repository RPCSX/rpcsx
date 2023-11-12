#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct MBusAVFile : orbis::File {};

static orbis::ErrorCode mbus_av_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled mbus_av ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = mbus_av_ioctl,
};

struct MBusAVDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<MBusAVFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createMBusAVCharacterDevice() { return orbis::knew<MBusAVDevice>(); }
