#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/thread/Thread.hpp"

struct DipswFile : public orbis::File {};

static orbis::ErrorCode dipsw_ioctl(orbis::File *file, std::uint64_t request,
                                    void *argp, orbis::Thread *thread) {
  if (request == 0x40048806) { // isDevelopmentMode
    ORBIS_LOG_ERROR("dipsw ioctl isDevelopmentMode", argp);

    *reinterpret_cast<std::uint32_t *>(argp) = 0;
    return {};
  }

  if (request == 0x40048807) {
    ORBIS_LOG_ERROR("dipsw ioctl 0x40048807", argp);
    *reinterpret_cast<std::uint32_t *>(argp) = 1;
    return {};
  }

  // 0x40088808
  // 0x40088809

  if (request == 0x40088808) {
    ORBIS_LOG_ERROR("dipsw ioctl 0x40088808", argp);
    *reinterpret_cast<std::uint32_t *>(argp) = 1;
    return {};
  }

  // 0x8010880a
  if (request == 0x8010880a) { // write data? used on initilization
    struct Args {
      std::uint64_t address;
      std::uint64_t size;
    };

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_ERROR("dipsw ioctl 0x8010880a", args->address, args->size);

    return {};
  }

  ORBIS_LOG_FATAL("Unhandled dipsw ioctl", request);
  thread->where();
  //__builtin_trap();
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = dipsw_ioctl,
};

struct DipswDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<DipswFile>();
    newFile->device = this;
    newFile->ops = &ops;
    *file = newFile;
    return {};
  }
};

IoDevice *createDipswCharacterDevice() { return orbis::knew<DipswDevice>(); }
