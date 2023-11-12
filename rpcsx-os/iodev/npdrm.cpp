#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct NpdrmFile : orbis::File {};

static orbis::ErrorCode npdrm_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled NPDRM ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = npdrm_ioctl,
};

struct NpdrmDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<NpdrmFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createNpdrmCharacterDevice() { return orbis::knew<NpdrmDevice>(); }
