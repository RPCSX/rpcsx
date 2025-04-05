#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include <thread>

struct XptFile : orbis::File {};

static orbis::ErrorCode xpt_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled xpt ioctl", request);
  if (request == 0xc4a81602) {
    thread->where();
    std::this_thread::sleep_for(std::chrono::hours(120));
  }
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = xpt_ioctl,
};

struct XptDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<XptFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createXptCharacterDevice() { return orbis::knew<XptDevice>(); }
