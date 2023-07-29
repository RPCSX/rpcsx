#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"

struct HmdMmapDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode) override;
};
struct HmdMmapFile : public orbis::File {};

static orbis::ErrorCode hmd_mmap_ioctl(orbis::File *file, std::uint64_t request,
                                       void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd_mmap ioctl", request);

  // 0x800c4802
  return {};
}

static orbis::ErrorCode hmd_mmap_mmap(orbis::File *file, void **address,
                                      std::uint64_t size, std::int32_t prot,
                                      std::int32_t flags, std::int64_t offset,
                                      orbis::Thread *thread) {
  ORBIS_LOG_FATAL("hmd_mmap mmap", address, size, offset);
  auto result = rx::vm::map(*address, size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd_mmap_ioctl,
    .mmap = hmd_mmap_mmap,
};

orbis::ErrorCode HmdMmapDevice::open(orbis::Ref<orbis::File> *file,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode) {
  auto newFile = orbis::knew<HmdMmapFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createHmdMmapCharacterDevice() {
  return orbis::knew<HmdMmapDevice>();
}
