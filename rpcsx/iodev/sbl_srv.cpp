#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/SharedMutex.hpp"

struct SblSrvFile : public orbis::File {};

struct SblSrvDevice : orbis::IoDevice {
  rx::shared_mutex mtx;
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode sbl_srv_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled sbl_srv ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = sbl_srv_ioctl,
};

orbis::ErrorCode SblSrvDevice::open(rx::Ref<orbis::File> *file,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<SblSrvFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::IoDevice *createSblSrvCharacterDevice() {
  return orbis::knew<SblSrvDevice>();
}
