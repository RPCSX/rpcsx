#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include <span>
#include <unistd.h>

struct ConsoleFile : orbis::File {};
struct ConsoleDevice : IoDevice {
  int inputFd;
  int outputFd;

  ConsoleDevice(int inputFd, int outputFd)
      : inputFd(inputFd), outputFd(outputFd) {}

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode console_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled console ioctl", request);
  return {};
}

static orbis::ErrorCode console_read(orbis::File *file, orbis::Uio *uio,
                                     orbis::Thread *thread) {
  auto dev = dynamic_cast<ConsoleDevice *>(file->device.get());

  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    auto result = ::read(dev->inputFd, vec.base, vec.len);

    if (result < 0) {
      return orbis::ErrorCode::IO;
    }

    uio->offset += result;
    break;
  }
  return {};
}

static orbis::ErrorCode console_write(orbis::File *file, orbis::Uio *uio,
                                      orbis::Thread *thread) {
  auto dev = dynamic_cast<ConsoleDevice *>(file->device.get());

  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    ::write(dev->outputFd, vec.base, vec.len);
  }
  uio->resid = 0;
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = console_ioctl,
    .read = console_read,
    .write = console_write,
};

orbis::ErrorCode ConsoleDevice::open(orbis::Ref<orbis::File> *file,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode,
                                     orbis::Thread *thread) {
  auto newFile = orbis::knew<ConsoleFile>();
  newFile->ops = &fileOps;
  newFile->device = this;

  *file = newFile;
  return {};
}

IoDevice *createConsoleCharacterDevice(int inputFd, int outputFd) {
  return orbis::knew<ConsoleDevice>(inputFd, outputFd);
}
