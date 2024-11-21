#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include <chrono>
#include <thread>

struct A53IoFile : orbis::File {};

static orbis::ErrorCode a53io_ioctl(orbis::File *file, std::uint64_t request,
                                    void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled a53io ioctl", request);
  return {};
}
static orbis::ErrorCode a53io_read(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *thread) {
  ORBIS_LOG_TODO(__FUNCTION__);

  std::this_thread::sleep_for(std::chrono::days(1));
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = a53io_ioctl,
    .read = a53io_read,
};

struct A53IoDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<A53IoFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createA53IoCharacterDevice() { return orbis::knew<A53IoDevice>(); }
