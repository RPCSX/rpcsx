#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"

struct Hmd2GazeDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct Hmd2GazeFile : public orbis::File {};

static orbis::ErrorCode hmd2_gaze_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd2_gaze ioctl", request);
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd2_gaze_ioctl,
};

orbis::ErrorCode Hmd2GazeDevice::open(orbis::Ref<orbis::File> *file,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<Hmd2GazeFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createHmd2GazeCharacterDevice() { return orbis::knew<Hmd2GazeDevice>(); }
