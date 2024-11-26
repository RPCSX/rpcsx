#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"

struct CameraFile : orbis::File {};

static orbis::ErrorCode camera_ioctl(orbis::File *file, std::uint64_t request,
                                     void *argp, orbis::Thread *thread) {
  if (request == 0xc0308e14) {
    struct Args {
      orbis::uint32_t unk0;
      orbis::uint32_t unk1;
      orbis::uint32_t unk2;
      orbis::uint32_t unk3;
      orbis::int32_t unk4;
      orbis::uint32_t unk5;
      orbis::uint32_t unk6;
    };
    auto args = reinterpret_cast<Args *>(argp);
    args->unk0 = 0xff;
    args->unk1 = 0;
    args->unk2 = 0;
    args->unk3 = 0;
    args->unk4 = 0;
    args->unk6 = 0x1337;
    return {};
  }

  ORBIS_LOG_FATAL("Unhandled camera ioctl", request);
  return {};
}

static orbis::ErrorCode camera_write(orbis::File *file, orbis::Uio *uio,
                                     orbis::Thread *thread) {
  // auto device = static_cast<azDevice *>(file->device.get());
  std::uint64_t totalSize = 0;
  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    totalSize += vec.len;
  }
  thread->retval[0] = totalSize;
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = camera_ioctl,
    .write = camera_write,
};

struct CameraDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<CameraFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createCameraCharacterDevice() { return orbis::knew<CameraDevice>(); }
