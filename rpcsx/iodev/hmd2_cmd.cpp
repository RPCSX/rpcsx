#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"

struct Hmd2CmdDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct Hmd2CmdFile : public orbis::File {};

static orbis::ErrorCode hmd2_cmd_ioctl(orbis::File *file, std::uint64_t request,
                                       void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd2_cmd ioctl", request);
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd2_cmd_ioctl,
};

orbis::ErrorCode Hmd2CmdDevice::open(orbis::Ref<orbis::File> *file,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode,
                                     orbis::Thread *thread) {
  auto newFile = orbis::knew<Hmd2CmdFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createHmd2CmdCharacterDevice() {
  return orbis::knew<Hmd2CmdDevice>();
}
