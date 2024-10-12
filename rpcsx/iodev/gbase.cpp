#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/thread/Thread.hpp"

struct GbaseFile : orbis::File {};

static orbis::ErrorCode gbase_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  if (request == 0xc0304510) {
    ORBIS_LOG_WARNING("gbase ioctl", request);
    struct Args {
      std::uint64_t unk0;
      std::uint64_t unk1;
      std::uint32_t currentClock;
      std::uint32_t unk2;
      std::uint64_t unk3;
      std::uint64_t unk4;
      std::uint64_t unk5;
    };

    static_assert(sizeof(Args) == 48);
    *reinterpret_cast<Args *>(argp) = {
      .currentClock = 1,
    };
    return{};
  }
  ORBIS_LOG_FATAL("Unhandled gbase ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = gbase_ioctl,
};

struct GbaseDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<GbaseFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createGbaseCharacterDevice() { return orbis::knew<GbaseDevice>(); }
