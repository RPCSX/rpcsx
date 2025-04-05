#include "io-device.hpp"
#include "orbis-config.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct AVControlFile : orbis::File {};

static orbis::ErrorCode av_control_ioctl(orbis::File *file,
                                         std::uint64_t request, void *argp,
                                         orbis::Thread *thread) {

  if (request == 0xc0109a0e) {
    struct Args {
      orbis::sint unk;
      orbis::sint padding;
      orbis::ptr<orbis::sint> pResult;
    };

    Args _args;
    ORBIS_RET_ON_ERROR(orbis::uread(_args, orbis::ptr<Args>(argp)));

    return orbis::uwrite(_args.pResult, 1);
  }

  ORBIS_LOG_FATAL("Unhandled av_control ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = av_control_ioctl,
};

struct AVControlDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<AVControlFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createAVControlCharacterDevice() {
  return orbis::knew<AVControlDevice>();
}
