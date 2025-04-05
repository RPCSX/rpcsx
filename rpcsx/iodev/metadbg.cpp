#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include <chrono>
#include <thread>

struct MetaDbgFile : orbis::File {};

static orbis::ErrorCode metadbg_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled metadbg ioctl", request);
  return {};
}
static orbis::ErrorCode metadbg_read(orbis::File *file, orbis::Uio *uio,
                                     orbis::Thread *thread) {
  ORBIS_LOG_TODO(__FUNCTION__);

  std::this_thread::sleep_for(std::chrono::days(1));
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = metadbg_ioctl,
    .read = metadbg_read,
};

struct MetaDbgDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<MetaDbgFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createMetaDbgCharacterDevice() {
  return orbis::knew<MetaDbgDevice>();
}
