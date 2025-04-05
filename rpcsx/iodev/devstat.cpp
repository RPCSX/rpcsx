#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct DevStatFile : orbis::File {};

static orbis::ErrorCode devstat_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled devstat ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = devstat_ioctl,
};

struct DevStatDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<DevStatFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createDevStatCharacterDevice() {
  return orbis::knew<DevStatDevice>();
}
