#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct RngDevice : public orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct RngFile : public orbis::File {};

static orbis::ErrorCode rng_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled rng ioctl", request);

  // 0x800c4802
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = rng_ioctl
};

orbis::ErrorCode RngDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                 std::uint32_t flags, std::uint32_t mode,
                                 orbis::Thread *thread) {
  auto newFile = orbis::knew<RngFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::IoDevice *createRngCharacterDevice() { return orbis::knew<RngDevice>(); }
