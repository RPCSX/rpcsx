#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct HidDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct HidFile : public orbis::File {};

static orbis::ErrorCode hid_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hid ioctl", request);

  // 0x800c4802
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hid_ioctl,
};

orbis::ErrorCode HidDevice::open(orbis::Ref<orbis::File> *file,
                                 const char *path, std::uint32_t flags,
                                 std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<HidFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createHidCharacterDevice() { return orbis::knew<HidDevice>(); }
