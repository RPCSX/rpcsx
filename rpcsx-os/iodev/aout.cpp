#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct AoutFile : orbis::File {};

static orbis::ErrorCode aout_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled aout ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = aout_ioctl,
};

struct AoutDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<AoutFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createAoutCharacterDevice() { return orbis::knew<AoutDevice>(); }
