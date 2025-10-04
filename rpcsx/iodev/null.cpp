#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/uio.hpp"
#include <span>

struct NullDevice : public IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct NullFile : public orbis::File {};

static orbis::ErrorCode null_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *) {
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    uio->offset += entry.len;
  }
  return {};
}

static const orbis::FileOps ops = {
    .write = null_write,
};

orbis::ErrorCode NullDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                  std::uint32_t flags, std::uint32_t mode,
                                  orbis::Thread *thread) {
  auto newFile = orbis::knew<NullFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createNullCharacterDevice() { return orbis::knew<NullDevice>(); }
