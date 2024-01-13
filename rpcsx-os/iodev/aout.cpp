#include "io-device.hpp"
#include "iodev/mbus_av.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/ProcessOps.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include <bits/types/struct_iovec.h>

struct AoutFile : orbis::File {};

static orbis::ErrorCode aout_ioctl(orbis::File *file, std::uint64_t request,
                                   void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled aout ioctl", request);
  thread->where();
  return {};
}

static orbis::ErrorCode aout_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *) {
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    uio->offset += entry.len;
  }
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = aout_ioctl,
    .write = aout_write,
};

struct AoutDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    ORBIS_LOG_FATAL("aout device open", path, flags, mode);
    auto newFile = orbis::knew<AoutFile>();
    newFile->ops = &fileOps;
    newFile->device = this;
    thread->where();

    *file = newFile;
    return {};
  }
};

IoDevice *createAoutCharacterDevice() { return orbis::knew<AoutDevice>(); }
