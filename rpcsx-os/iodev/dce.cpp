#include "io-device.hpp"
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "vm.hpp"

struct VideoOutBuffer {
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode;
  std::uint32_t pitch;
  std::uint32_t width;
  std::uint32_t height;
};

struct DceDevice : public IoDevice {};

// template <typename T>
// inline bool
// atomic_compare_exchange_weak(volatile T *ptr, T *expected, T desired,
//                              int successMemOrder = __ATOMIC_SEQ_CST,
//                              int failureMemOrder = __ATOMIC_SEQ_CST) {
//   return __atomic_compare_exchange_n(ptr, expected, desired, true,
//                                      __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
// }

struct DceInstance : public IoDeviceInstance {
  VideoOutBuffer bufferAttributes{};

  // std::uint64_t flipStatusOffset =
  //     mem::allocateInternal(sizeof(liverpool::bridge::FlipStatus),
  //                           alignof(liverpool::bridge::FlipStatus));
  // liverpool::bridge::FlipStatus *flipStatus = new (
  //     mem::mapInternal(flipStatusOffset, sizeof(liverpool::bridge::FlipStatus)))
  //     liverpool::bridge::FlipStatus();

  DceInstance() {
    // *flipStatus = {};
    // orbis::bridge.sendSetFlipStatus(flipStatusOffset);
  }
  void registerBuffer(int index, std::uint64_t address) {
    // orbis::bridge.sendSetBuffer(index, address, bufferAttributes.width,
    //                             bufferAttributes.height, bufferAttributes.pitch,
    //                             bufferAttributes.pixelFormat,
    //                             bufferAttributes.tilingMode);
  }

  void flip(std::uint32_t bufferIndex, std::uint64_t flipMode,
            std::uint64_t flipArg) {

    // orbis::bridge.sendFlip(bufferIndex, flipArg);
    // orbis::bridge.wait();
  }

  // liverpool::bridge::FlipStatus getFlipStatus() {
  //   int expected = 0;
  //   while (!atomic_compare_exchange_weak(&flipStatus->locked, &expected, 1)) {
  //     expected = 0;
  //   }

  //   liverpool::bridge::FlipStatus result = *flipStatus;
  //   flipStatus->locked = 0;

  //   return result;
  // }
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

static std::int64_t dce_instance_ioctl(IoDeviceInstance *instance,
                                       std::uint64_t request, void *argp) {
  auto dceInstance = static_cast<DceInstance *>(instance);
  static std::uint64_t *bufferInUsePtr = nullptr;

  if (request == 0xc0308203) {
    // flip control
    auto args = reinterpret_cast<FlipControlArgs *>(argp);

    std::printf("dce: FlipControl(%d, %lx, %p, %lx)\n", args->id, args->arg2,
                args->ptr, args->size);

    if (args->id == 6) { // set flip rate?
      std::printf("dce: FlipControl(set flip rate, %lx, %p, %lx)\n", args->arg2,
                  args->ptr, args->size);
    } else if (args->id == 10) {
      if (args->size != sizeof(FlipControlStatus)) {
        return 0;
      }

      // auto currentStatus = dceInstance->getFlipStatus();

      FlipControlStatus flipStatus{};
      // flipStatus.flipArg = currentStatus.arg;
      // flipStatus.count = currentStatus.count;
      flipStatus.processTime = 0; // TODO
      flipStatus.tsc = 0;         // TODO
      // flipStatus.currentBuffer = currentStatus.currentBuffer;
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
      std::printf("dce: FlipControl allocate(%u, %lx, %p, %lx)\n", args->id,
                  args->arg2, args->ptr, args->size);
      *(std::uint64_t *)args->ptr = 0;         // dev offset
      *(std::uint64_t *)args->size = 0x100000; // size
    } else if (args->id == 31) {
      bufferInUsePtr = (std::uint64_t *)args->size;
      std::printf("flipStatusPtr = %p\n", bufferInUsePtr);
      return 0;
    } else if (args->id != 0 && args->id != 1) { // used during open/close
      std::printf("dce: UNIMPLEMENTED FlipControl(%u, %lx, %p, %lx)\n",
                  args->id, args->arg2, args->ptr, args->size);

      std::fflush(stdout);
      __builtin_trap();
    }
    return 0;
  }

  if (request == 0xc0308206) {
    auto args = reinterpret_cast<RegisterBuffer *>(argp);
    std::fprintf(stderr, "dce: RegisterBuffer(%lx, %lx, %lx, %lx)\n",
                 args->attributeIndex, args->index, args->address, args->unk);

    dceInstance->registerBuffer(args->index, args->address);
    return 0;
  }

  if (request == 0xc0308207) { // SCE_SYS_DCE_IOCTL_REGISTER_BUFFER_ATTRIBUTE
    auto args = reinterpret_cast<RegisterBufferAttributeArgs *>(argp);

    std::fprintf(
        stderr,
        "dce: RegisterBufferAttributes(unk0=%lx, unk1=%x, unk2_flag=%x, "
        "unk3=%x, "
        "pixelFormat=%x, tilingMode=%x, pitch=%u, width=%u, "
        "height=%u, "
        "unk4_zero=%x, unk5_zero=%x, unk6=%x, unk7_-1=%lx, unk8=%x)\n",
        args->unk0, args->unk1, args->unk2_flag, args->unk3, args->pixelFormat,
        args->tilingMode, args->pitch, args->width, args->height,
        args->unk4_zero, args->unk5_zero, args->unk6, args->unk7, args->unk8);

    dceInstance->bufferAttributes.pixelFormat = args->pixelFormat;
    dceInstance->bufferAttributes.tilingMode = args->tilingMode;
    dceInstance->bufferAttributes.pitch = args->pitch;
    dceInstance->bufferAttributes.width = args->width;
    dceInstance->bufferAttributes.height = args->height;
    return 0;
  }

  if (request == 0xc0488204) {
    // flip request
    auto args = reinterpret_cast<FlipRequestArgs *>(argp);

    std::fprintf(
        stderr,
        "dce: FlipRequestArgs(%lx, displayBufferIndex = %x, flipMode = %lx, "
        "flipArg = %lx, "
        "%x, %x, %x, "
        "%x)\n",
        args->arg1, args->displayBufferIndex, args->flipMode, args->flipArg,
        args->arg5, args->arg6, args->arg7, args->arg8);

    dceInstance->flip(args->displayBufferIndex, args->flipMode, args->flipArg);

    if (args->flipMode == 1 || args->arg7 == 0) {
      // orbis::bridge.sendDoFlip();
    }


    if (args->displayBufferIndex != -1) {
      if (bufferInUsePtr) {
        auto ptr = bufferInUsePtr + args->displayBufferIndex;
        std::printf(" ========== fill status to %p\n", ptr);
        *ptr = 0;
      }
    }
    return 0;
  }

  if (request == 0x80088209) { // deallocate?
    auto arg = *reinterpret_cast<std::uint64_t *>(argp);
    std::fprintf(stderr, "dce: 0x80088209(%lx)\n", arg);
    return 0;
  }

  std::fprintf(stderr, "***ERROR*** Unhandled dce ioctl %lx\n", request);
  std::fflush(stdout);
  __builtin_trap();
  return 0;
}

static void *dce_instance_mmap(IoDeviceInstance *instance, void *address,
                               std::uint64_t size, std::int32_t prot,
                               std::int32_t flags, std::int64_t offset) {
  std::fprintf(stderr, "dce mmap: address=%p, size=%lx, offset=%lx\n", address,
               size, offset);
  return rx::vm::map(address, size, prot, flags);
}

static std::int32_t dce_device_open(IoDevice *device,
                                    orbis::Ref<IoDeviceInstance> *instance,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode) {
  auto *newInstance = new DceInstance();
  newInstance->ioctl = dce_instance_ioctl;
  newInstance->mmap = dce_instance_mmap;
  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createDceCharacterDevice() {
  auto *newDevice = new DceDevice();
  newDevice->open = dce_device_open;
  return newDevice;
}
