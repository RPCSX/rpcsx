#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct AjmFile : orbis::File {};

static orbis::ErrorCode ajm_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled AJM ioctl", request);
  return orbis::ErrorCode::PERM;
}

static const orbis::FileOps fileOps = {
    .ioctl = ajm_ioctl,
};

struct AjmDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<AjmFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createAjmCharacterDevice() { return orbis::knew<AjmDevice>(); }
