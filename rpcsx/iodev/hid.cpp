#include "gpu/DeviceCtl.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include <chrono>

struct HidDevice : public IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct HidFile : public orbis::File {};

struct PadState {
  std::uint64_t timestamp;
  std::uint32_t unk;
  std::uint32_t buttons;
  std::uint8_t leftStickX;
  std::uint8_t leftStickY;
  std::uint8_t rightStickX;
  std::uint8_t rightStickY;
  std::uint8_t l2;
  std::uint8_t r2;
};

static orbis::ErrorCode hid_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {
  switch (request) {
  case 0x800c4802:
    ORBIS_LOG_FATAL("hid ioctl", request);
    thread->retval[0] = 1; // hid id
    return {};

  case 0x8030482e: {
    // ORBIS_LOG_FATAL("hid ioctl", request);
    // read state
    struct ReadStateArgs {
      std::uint32_t hidId;
      std::uint32_t unk0;
      amdgpu::PadState *state;
      std::uint32_t unk2;
      std::uint32_t *connected;
      std::uint32_t *unk4;
      std::uint64_t unk5;
    };

    auto args = *reinterpret_cast<ReadStateArgs *>(argp);
    // ORBIS_LOG_ERROR("hid read state", args.hidId, args.unk0, args.state,
    //                 args.unk2, args.connected, args.unk4, args.unk5);

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      *args.state = gpu.getContext().kbPadState;
      *args.connected = 1;
      *args.unk4 = 1; // is wireless?
      thread->retval[0] = 1;
    }
    return {};
  }

  case 0x8010480e: {
    // read information
    ORBIS_LOG_ERROR("hid read information");
    return {};
  }

  case 0x80204829: {
    struct MiniReadStateArgs {
      orbis::uint hidId;
      orbis::uint unk0;
      orbis::ptr<amdgpu::PadState> state;
      orbis::uint count;
      orbis::uint padding;
      orbis::ptr<orbis::uint> unk5;
    };
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      auto args = *reinterpret_cast<MiniReadStateArgs *>(argp);
      *args.state = gpu.getContext().kbPadState;
      thread->retval[0] = 1;
    }
    return {};
  }

  default:
    ORBIS_LOG_FATAL("Unhandled hid ioctl", request);
    thread->where();
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
