#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct VCEFile : orbis::File {};

static orbis::ErrorCode vce_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  switch (request) {
  case 0xc0048406:
    *reinterpret_cast<std::uint32_t *>(argp) = 0x700;
    return {};

  case 0x80488401:
    auto unkAddress = *reinterpret_cast<std::uint64_t *>(argp);
    ORBIS_LOG_ERROR(__FUNCTION__, request, unkAddress);
    return {};
  }

  ORBIS_LOG_FATAL("Unhandled vce ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = vce_ioctl,
};

struct VCEDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<VCEFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createVCECharacterDevice() { return orbis::knew<VCEDevice>(); }
