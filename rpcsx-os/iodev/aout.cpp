#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include <chrono>
#include <thread>

struct AoutFile : orbis::File {};

static orbis::ErrorCode aout_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled aout ioctl", request);
  if (request == 0xc004500a) {
    std::this_thread::sleep_for(std::chrono::days(1));
  }
  return {};
}

static orbis::ErrorCode aout_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *) {
  uio->resid = 0;
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = aout_ioctl,
    .write = aout_write,
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
