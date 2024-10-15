#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/uio.hpp"
#include <cstring>
#include <span>

struct UrandomDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct UrandomFile : public orbis::File {};

static orbis::ErrorCode urandom_read(orbis::File *file, orbis::Uio *uio,
                                  orbis::Thread *) {
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    std::memset(entry.base, 0, entry.len);
    uio->offset += entry.len;
  }
  uio->resid = 0;
  return {};
}

static const orbis::FileOps ops = {
    .read = urandom_read,
};

orbis::ErrorCode UrandomDevice::open(orbis::Ref<orbis::File> *file,
                                  const char *path, std::uint32_t flags,
                                  std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<UrandomFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createUrandomCharacterDevice() { return orbis::knew<UrandomDevice>(); }
