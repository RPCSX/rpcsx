#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
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

  ORBIS_LOG_FATAL("hid ioctl", request);
  switch (request) {
  case 0x800c4802:
    thread->retval[0] = 1; // hid id
    return {};

  case 0x8030482e: {
    // read state
    struct ReadStateArgs {
      std::uint32_t hidId;
      std::uint32_t unk0;
      void *unk1;
      std::uint32_t unk2;
      void *unk3;
      void *unk4;
      std::uint64_t unk5;
    };

    auto args = *reinterpret_cast<ReadStateArgs *>(argp);
    ORBIS_LOG_ERROR("hid read state", args.hidId, args.unk0, args.unk1,
                    args.unk2, args.unk3, args.unk4, args.unk5);
  }

  default:
    ORBIS_LOG_FATAL("Unhandled hid ioctl", request);
  }

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
