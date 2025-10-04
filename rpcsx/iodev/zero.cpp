#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/uio.hpp"
#include <cstring>
#include <span>

struct ZeroDevice : public IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct ZeroFile : public orbis::File {};

static orbis::ErrorCode zero_read(orbis::File *file, orbis::Uio *uio,
                                  orbis::Thread *) {
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    std::memset(entry.base, 0, entry.len);
    uio->offset += entry.len;
  }
  uio->resid = 0;
  return {};
}

static const orbis::FileOps ops = {
    .read = zero_read,
};

orbis::ErrorCode ZeroDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                  std::uint32_t flags, std::uint32_t mode,
                                  orbis::Thread *thread) {
  auto newFile = orbis::knew<ZeroFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createZeroCharacterDevice() { return orbis::knew<ZeroDevice>(); }
