#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct Hmd3daDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode) override;
};
struct Hmd3daFile : public orbis::File {};

static orbis::ErrorCode hmd_3da_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd_3da ioctl", request);

  // 0x800c4802
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd_3da_ioctl,
};

orbis::ErrorCode Hmd3daDevice::open(orbis::Ref<orbis::File> *file,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode) {
  auto newFile = orbis::knew<Hmd3daFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createHmd3daCharacterDevice() { return orbis::knew<Hmd3daDevice>(); }
