#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct AjmFile : orbis::File {};

static orbis::ErrorCode ajm_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  // 0xc0288903 - module register
  // 0xc0288904 - module unregister
  if (request == 0xc0288903 || request == 0xc0288904) {
    auto arg = reinterpret_cast<std::uint32_t *>(argp)[2];
    ORBIS_LOG_ERROR(__FUNCTION__, request, arg);
    *reinterpret_cast<std::uint64_t *>(argp) = 0;
    // return{};
  }

  ORBIS_LOG_FATAL("Unhandled AJM ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = ajm_ioctl,
};

struct AjmDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<AjmFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createAjmCharacterDevice() { return orbis::knew<AjmDevice>(); }
