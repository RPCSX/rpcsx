#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"

struct RngDevice : public IoDevice {
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

static orbis::ErrorCode rng_mmap(orbis::File *file, void **address,
                                 std::uint64_t size, std::int32_t prot,
                                 std::int32_t flags, std::int64_t offset,
                                 orbis::Thread *thread) {
  ORBIS_LOG_FATAL("rng mmap", address, size, offset);
  auto result = vm::map(*address, size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = rng_ioctl,
    .mmap = rng_mmap,
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

IoDevice *createRngCharacterDevice() { return orbis::knew<RngDevice>(); }
