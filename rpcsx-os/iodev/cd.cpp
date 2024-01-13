#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct CdFile : orbis::File {};

static orbis::ErrorCode cd_ioctl(orbis::File *file, std::uint64_t request,
                                 void *argp, orbis::Thread *thread) {
  if (request == 0xc4a81602) {
    (*(std::uint32_t *)((char *)argp + 0x54)) = 1;
    // std::this_thread::sleep_for(std::chrono::hours(120));
    return {};
  }

  ORBIS_LOG_FATAL("Unhandled cd ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = cd_ioctl,
};

struct CdDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<CdFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createCdCharacterDevice() { return orbis::knew<CdDevice>(); }
