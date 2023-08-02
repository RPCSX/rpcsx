#include "bridge.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "vm.hpp"
#include <cstdio>
#include <cstring>
#include <mutex>

struct VideoOutBuffer {
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode;
  std::uint32_t pitch;
  std::uint32_t width;
  std::uint32_t height;
};

struct RegisterBuffer {
  std::uint64_t attributeIndex;
  std::uint64_t index;
  std::uint64_t address;
  std::uint64_t unk;
};

struct RegisterBufferAttributeArgs {
  std::uint64_t unk0;
  std::uint8_t unk1;
  std::uint8_t unk2_flag;
  std::uint16_t unk3; // 0
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode; // 1 // tilingMode?
  std::uint32_t pitch;
  std::uint32_t width;
  std::uint32_t height;
  std::uint8_t unk4_zero; // 0
  std::uint8_t unk5_zero; // 0
  std::uint16_t unk6;
  std::uint64_t unk7; // -1
  std::uint32_t unk8;
};

struct FlipRequestArgs {
  std::uint64_t arg1;
  std::int32_t displayBufferIndex;
  std::uint64_t flipMode; // flip mode?
  std::uint64_t flipArg;
  std::uint32_t arg5;
  std::uint32_t arg6;
  std::uint32_t arg7;
  std::uint32_t arg8;
};

struct FlipControlStatus {
  std::uint64_t flipArg;
  std::uint64_t unk0;
  std::uint64_t count;
  std::uint64_t processTime;
  std::uint64_t tsc;
  std::uint32_t currentBuffer;
  std::uint32_t unkQueueNum;
  std::uint32_t gcQueueNum;
  std::uint32_t unk2QueueNum;
  std::uint32_t submitTsc;
  std::uint64_t unk1;
};

struct FlipControlArgs {
  std::uint32_t id;
  // std::uint32_t padding;
  std::uint64_t arg2;
  void *ptr;
  std::uint64_t size; // 0x48 // size?
};

struct ResolutionStatus {
  std::uint32_t width;
  std::uint32_t heigth;
  std::uint32_t x;
  std::uint32_t y;
};

struct DceFile : public orbis::File {};

struct DceDevice : IoDevice {
  orbis::shared_mutex mtx;
  VideoOutBuffer bufferAttributes{}; // TODO
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode dce_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {
  auto device = static_cast<DceDevice *>(file->device.get());

  std::lock_guard lock(device->mtx);

  if (request == 0xc0308203) {
    // returns:
    // PERM
    // NOMEM
    // FAULT
    // BUSY
    // INVAL
    // OPNOTSUPP
    // flip control

    // 0xc - scaler ctl
    // 0x11 - output ctl
    // 0x12 - vblank ctl
    // 0x14 - display ctl
    // 0x15 - subwindow ctl
    // 0x18 - cursor ctl
    // 0x19 - port ctl
    // 0x1d - color ctl
    // 0x1f - config ctl
    // 0x20 - zoom buffer ctl
    // 0x21 - adjust color
    auto args = reinterpret_cast<FlipControlArgs *>(argp);

    ORBIS_LOG_NOTICE("dce: FlipControl", args->id, args->arg2, args->ptr,
                     args->size);

    if (args->id == 6) { // set flip rate?
      ORBIS_LOG_NOTICE("dce: FlipControl: set flip rate", args->arg2, args->ptr,
                       args->size);
    } else if (args->id == 10) {
      if (args->size != sizeof(FlipControlStatus)) {
        return {};
      }

      FlipControlStatus flipStatus{};
      // TODO: lock bridge header
      flipStatus.flipArg = rx::bridge.header->flipArg;
      flipStatus.count = rx::bridge.header->flipCount;
      flipStatus.processTime = 0; // TODO
      flipStatus.tsc = 0;         // TODO
      flipStatus.currentBuffer = rx::bridge.header->flipBuffer;
      flipStatus.unkQueueNum = 0;  // TODO
      flipStatus.gcQueueNum = 0;   // TODO
      flipStatus.unk2QueueNum = 0; // TODO
      flipStatus.submitTsc = 0;    // TODO

      std::memcpy(args->ptr, &flipStatus, sizeof(FlipControlStatus));
    } else if (args->id == 12) {
      *(std::uint64_t *)args->ptr = 0;
    } else if (args->id == 19) {
      // get resolution status
      auto status = (ResolutionStatus *)args->ptr;
      status->width = 1920;
      status->heigth = 1080;
      status->x = 0;
      status->y = 0;
    } else if (args->id == 9) {
      ORBIS_LOG_NOTICE("dce: FlipControl allocate", args->id, args->arg2,
                       args->ptr, args->size);
      *(std::uint64_t *)args->ptr = 0;         // dev offset
      *(std::uint64_t *)args->size = 0x100000; // size
    } else if (args->id == 31) {
      rx::bridge.header->bufferInUseAddress = args->size;
      return {};
    } else if (args->id == 33) { // adjust color
      std::printf("adjust color\n");
      return {};
    } else if (args->id != 0 && args->id != 1) { // used during open/close
      ORBIS_LOG_NOTICE("dce: UNIMPLEMENTED FlipControl", args->id, args->arg2,
                       args->ptr, args->size);

      std::fflush(stdout);
      //__builtin_trap();
    }
    return {};
  }

  if (request == 0xc0308206) {
    auto args = reinterpret_cast<RegisterBuffer *>(argp);
    ORBIS_LOG_ERROR("dce: RegisterBuffer", args->attributeIndex, args->index,
                    args->address, args->unk);

    if (args->index >= std::size(rx::bridge.header->buffers)) {
      // TODO
      ORBIS_LOG_FATAL("dce: out of buffers!");
      return orbis::ErrorCode::NOMEM;
    }

    // TODO: lock bridge header
    rx::bridge.header->buffers[args->index] = {
        .width = device->bufferAttributes.width,
        .height = device->bufferAttributes.height,
        .pitch = device->bufferAttributes.pitch,
        .address = args->address,
        .pixelFormat = device->bufferAttributes.pixelFormat,
        .tilingMode = device->bufferAttributes.tilingMode};
    return {};
  }

  if (request == 0xc0308207) { // SCE_SYS_DCE_IOCTL_REGISTER_BUFFER_ATTRIBUTE
    auto args = reinterpret_cast<RegisterBufferAttributeArgs *>(argp);

    ORBIS_LOG_ERROR("dce: RegisterBufferAttributes", args->unk0, args->unk1,
                    args->unk2_flag, args->unk3, args->pixelFormat,
                    args->tilingMode, args->pitch, args->width, args->height,
                    args->unk4_zero, args->unk5_zero, args->unk6, args->unk7,
                    args->unk8);

    device->bufferAttributes.pixelFormat = args->pixelFormat;
    device->bufferAttributes.tilingMode = args->tilingMode;
    device->bufferAttributes.pitch = args->pitch;
    device->bufferAttributes.width = args->width;
    device->bufferAttributes.height = args->height;
    return {};
  }

  if (request == 0xc0488204) {
    // flip request
    auto args = reinterpret_cast<FlipRequestArgs *>(argp);

    ORBIS_LOG_ERROR("dce: FlipRequestArgs", args->arg1,
                    args->displayBufferIndex, args->flipMode, args->flipArg,
                    args->arg5, args->arg6, args->arg7, args->arg8);

    rx::bridge.sendFlip(args->displayBufferIndex,
                        /*args->flipMode,*/ args->flipArg);
    return {};
  }

  if (request == 0x80088209) { // deallocate?
    auto arg = *reinterpret_cast<std::uint64_t *>(argp);
    ORBIS_LOG_ERROR("dce: 0x80088209", arg);
    return {};
  }

  ORBIS_LOG_FATAL("Unhandled dce ioctl", request);
  // 0xc0188213 - color conversion

  std::fflush(stdout);
  __builtin_trap();
  return {};
}

static orbis::ErrorCode dce_mmap(orbis::File *file, void **address,
                                 std::uint64_t size, std::int32_t prot,
                                 std::int32_t flags, std::int64_t offset,
                                 orbis::Thread *thread) {
  ORBIS_LOG_FATAL("dce mmap", address, size, offset);
  auto result = rx::vm::map(*address, size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = dce_ioctl,
    .mmap = dce_mmap,
};

orbis::ErrorCode DceDevice::open(orbis::Ref<orbis::File> *file,
                                 const char *path, std::uint32_t flags,
                                 std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<DceFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createDceCharacterDevice() { return orbis::knew<DceDevice>(); }
