#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct DevActFile : orbis::File {};

static orbis::ErrorCode devact_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  if (request == 0x40105303) {
    // is expired
    struct Param {
      std::uint32_t unk0;
      std::uint32_t unk1;
      std::uint32_t unk2;
      std::uint32_t unk3;
    };
    auto param = (Param *)argp;
    *param = {};
    param->unk0 = 1;
    return{};
  }
  ORBIS_LOG_FATAL("Unhandled devact ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = devact_ioctl,
};

struct DevActDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<DevActFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createDevActCharacterDevice() { return orbis::knew<DevActDevice>(); }
