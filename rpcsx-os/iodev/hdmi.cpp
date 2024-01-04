#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct HDMIFile : orbis::File {};

static orbis::ErrorCode hdmi_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled hdmi ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = hdmi_ioctl,
};

struct HDMIDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<HDMIFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createHDMICharacterDevice() { return orbis::knew<HDMIDevice>(); }
