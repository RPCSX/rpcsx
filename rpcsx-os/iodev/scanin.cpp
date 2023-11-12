#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct ScaninFile : orbis::File {};

static orbis::ErrorCode scanin_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled scanin ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = scanin_ioctl,
};

struct ScaninDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<ScaninFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createScaninCharacterDevice() { return orbis::knew<ScaninDevice>(); }
