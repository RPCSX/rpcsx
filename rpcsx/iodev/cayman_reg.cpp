#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct CaymanRegDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode cayman_reg_ioctl(orbis::File *file,
                                         std::uint64_t request, void *argp,
                                         orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled cayman_reg ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = cayman_reg_ioctl,
};

orbis::ErrorCode CaymanRegDevice::open(orbis::Ref<orbis::File> *file,
                                       const char *path, std::uint32_t flags,
                                       std::uint32_t mode,
                                       orbis::Thread *thread) {
  auto newFile = orbis::knew<orbis::File>();
  newFile->ops = &fileOps;
  newFile->device = this;

  *file = newFile;
  return {};
}

IoDevice *createCaymanRegCharacterDevice() {
  return orbis::knew<CaymanRegDevice>();
}
