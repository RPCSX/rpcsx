#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct IccConfigurationFile : orbis::File {};

static orbis::ErrorCode icc_configuration_ioctl(orbis::File *file,
                                                std::uint64_t request,
                                                void *argp,
                                                orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled icc_configuration ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = icc_configuration_ioctl,
};

struct IccConfigurationDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<IccConfigurationFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createIccConfigurationCharacterDevice() {
  return orbis::knew<IccConfigurationDevice>();
}
