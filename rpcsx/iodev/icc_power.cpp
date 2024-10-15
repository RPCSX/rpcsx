#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct IccPowerDevice : IoDevice {
  std::uint8_t bootphase = 0;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode icc_power_ioctl(orbis::File *file,
                                        std::uint64_t request, void *argp,
                                        orbis::Thread *thread) {

  // 0xc0019901 - bootphase set
  // 0xc0099902 - unk
  // 0x40019907 - bootphase get

  auto iccPower = file->device.staticCast<IccPowerDevice>();

  switch (request) {
  case 0xc0019901: {
    iccPower->bootphase = *reinterpret_cast<std::uint8_t *>(argp);
    ORBIS_LOG_WARNING(__FUNCTION__, request, iccPower->bootphase);
    return{};
  }

  case 0xc0099902: {
    auto &unk = *reinterpret_cast<std::uint32_t *>(argp);
    ORBIS_LOG_WARNING(__FUNCTION__, request, unk);
    unk = 1;
    return{};
  }

  case 0x40019907:
    ORBIS_LOG_WARNING(__FUNCTION__, request);
    *reinterpret_cast<std::uint8_t *>(argp) = iccPower->bootphase;
    return{};
  }

  ORBIS_LOG_FATAL("Unhandled icc_power ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = icc_power_ioctl,
};

orbis::ErrorCode IccPowerDevice::open(orbis::Ref<orbis::File> *file,
                                      const char *path, std::uint32_t flags,
                                      std::uint32_t mode,
                                      orbis::Thread *thread) {
  auto newFile = orbis::knew<orbis::File>();
  newFile->ops = &fileOps;
  newFile->device = this;

  *file = newFile;
  return {};
}

IoDevice *createIccPowerCharacterDevice() {
  return orbis::knew<IccPowerDevice>();
}
