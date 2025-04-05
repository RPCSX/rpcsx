#include "../io-devices.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include "vfs.hpp"
#include <chrono>
#include <thread>

struct NsidCtlFile : orbis::File {};

static orbis::ErrorCode nsid_ctl_ioctl(orbis::File *file, std::uint64_t request,
                                       void *argp, orbis::Thread *thread) {

  if (request == 0x20003102) {
    ORBIS_LOG_ERROR(__FUNCTION__, "create ssd0 device");
    vfs::addDevice("ssd0", createHddCharacterDevice(0x100000000));
  } else {
    ORBIS_LOG_FATAL("Unhandled nsid_ctl ioctl", request);
  }

  return {};
}
static orbis::ErrorCode nsid_ctl_read(orbis::File *file, orbis::Uio *uio,
                                      orbis::Thread *thread) {
  ORBIS_LOG_TODO(__FUNCTION__);

  std::this_thread::sleep_for(std::chrono::days(1));
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = nsid_ctl_ioctl,
    .read = nsid_ctl_read,
};

struct NsidCtlDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<NsidCtlFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createNsidCtlCharacterDevice() {
  return orbis::knew<NsidCtlDevice>();
}
