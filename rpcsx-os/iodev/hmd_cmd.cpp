#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"

struct HmdCmdDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct HmdCmdFile : public orbis::File {};

static orbis::ErrorCode hmd_cmd_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd_cmd ioctl", request);

  // 0x800c4802
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd_cmd_ioctl,
};

orbis::ErrorCode HmdCmdDevice::open(orbis::Ref<orbis::File> *file,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<HmdCmdFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createHmdCmdCharacterDevice() { return orbis::knew<HmdCmdDevice>(); }
