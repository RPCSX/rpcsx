#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include <span>
#include <unistd.h>

struct EvlgFile : orbis::File {};
struct EvlgDevice : IoDevice {
  int outputFd;

  EvlgDevice(int outputFd) : outputFd(outputFd) {}

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode evlg_ioctl(orbis::File *file, std::uint64_t request,
                                   void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled evlg ioctl", request);
  return {};
}

static orbis::ErrorCode evlg_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *thread) {
  auto dev = dynamic_cast<EvlgDevice *>(file->device.get());

  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    uio->offset += vec.len;
    ::write(dev->outputFd, vec.base, vec.len);
    ::write(2, vec.base, vec.len);
  }
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = evlg_ioctl,
    .write = evlg_write,
};

orbis::ErrorCode EvlgDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                  std::uint32_t flags, std::uint32_t mode,
                                  orbis::Thread *thread) {
  auto newFile = orbis::knew<EvlgFile>();
  newFile->ops = &fileOps;
  newFile->device = this;

  *file = newFile;
  return {};
}

IoDevice *createEvlgCharacterDevice(int outputFd) {
  return orbis::knew<EvlgDevice>(outputFd);
}
