#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct S3DAFile : orbis::File {};

static orbis::ErrorCode s3da_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled s3da ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = s3da_ioctl,
};

struct S3DADevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<S3DAFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createS3DACharacterDevice() { return orbis::knew<S3DADevice>(); }
