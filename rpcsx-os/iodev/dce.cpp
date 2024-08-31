#include "bridge.hpp"
#include "io-device.hpp"
#include "iodev/dmem.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "vm.hpp"
#include <cstdio>
#include <cstring>
#include <mutex>

static constexpr auto kDceControlMemoryOffset = 0;
static constexpr auto kDceControlMemorySize = 0x10000;

struct VideoOutBuffer {
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode;
  std::uint32_t pitch;
  std::uint32_t width;
  std::uint32_t height;
};

struct RegisterBuffer {
  std::uint64_t canary;   // arg5 data in FlipControlArgs:0: *arg5
  std::uint32_t index;    // buffer index [0..15]
  std::uint32_t attrid;   // attribute id [0..3]
  std::uint64_t address;  // left buffer ptr
  std::uint64_t address2; // right buffer ptr (Stereo)
};

struct RegisterBufferAttributeArgs {
  std::uint64_t canary; // arg5 data in FlipControlArgs:0: *arg5
  std::uint8_t attrid;  // attribute id [0..3]
  std::uint8_t submit;  // 0 = RegisterBuffers ; 1 = SubmitChangeBufferAttribute
  std::uint16_t unk3;   // 0
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode; // 1 // tilingMode?
  std::uint32_t pitch;
  std::uint32_t width;
  std::uint32_t height;
  std::uint8_t unk4_zero; // 0
  std::uint8_t unk5_zero; // 0
  std::uint16_t options;
  std::uint64_t reserved1; // -1
  std::uint32_t reserved2;
};

struct FlipRequestArgs {            // submit_flip
  std::uint64_t canary;             // arg5 data in FlipControlArgs:0: *arg5
  std::uint64_t displayBufferIndex; //[0..15]
  std::uint32_t flipMode;           // flip mode?
  std::uint32_t unk1;
  std::uint64_t flipArg;
  std::uint64_t flipArg2; // not used
  std::uint32_t eop_nz;
  std::uint32_t unk2;
  std::uint64_t
      *eop_val; // reply with eop token if eop_nz=1 and send to wait eop
  std::uint64_t unk3;
  std::uint64_t *rout; // extraout of result error
};

struct FlipControlStatus {
  std::uint64_t flipArg;
  std::uint64_t flipArg2; // not used
  std::uint64_t count;
  std::uint64_t processTime;
  std::uint64_t tsc;
  std::uint32_t currentBuffer;
  std::uint32_t flipPendingNum0; // flipPendingNum = flipPendingNum0 +
                                 // gcQueueNum + flipPendingNum1
  std::uint32_t gcQueueNum;
  std::uint32_t flipPendingNum1;
  std::uint64_t submitTsc;
  std::uint64_t unk1;
};

struct FlipControlArgs {
  std::uint32_t id;
  std::uint32_t padding;
  std::uint64_t arg2;
  void *ptr;
  std::uint64_t size; // 0x48 // size?
  std::uint64_t arg5;
  std::uint64_t arg6;
};

struct ResolutionStatus {
  std::uint32_t width;
  std::uint32_t heigth;
  std::uint32_t paneWidth;
  std::uint32_t paneHeight;
  std::uint32_t refreshHz;        // float
  std::uint32_t screenSizeInInch; // float
  std::byte padding[20];
};

// clang-format off
// refreshRate =    0                                        REFRESH_RATE_UNKNOWN
// refreshRate =    3; result.refreshHz = 0x426fc28f( 59.94) REFRESH_RATE_59_94HZ
// refreshRate =    2, result.refreshHz = 0x42480000( 50.00) REFRESH_RATE_50HZ
// refreshRate =    1, result.refreshHz = 0x41bfd70a( 23.98) REFRESH_RATE_23_98HZ
// refreshRate =    4, result.refreshHz = 0x41c00000( 24.00)
// refreshRate =    5, result.refreshHz = 0x41f00000( 30.00)
// refreshRate =    6, result.refreshHz = 0x41efc28f( 29.97) REFRESH_RATE_29_97HZ
// refreshRate =    7, result.refreshHz = 0x41c80000( 25.00)
// refreshRate =    9, result.refreshHz = 0x42700000( 60.00)
// refreshRate =   10, result.refreshHz = 0x42400000( 48.00)
// refreshRate =  0xb, result.refreshHz = 0x423fcccd( 47.95)
// refreshRate =  0xc, result.refreshHz = 0x42c80000(100.00)
// refreshRate =  0xd, result.refreshHz = 0x42efc28f(119.88) REFRESH_RATE_119_88HZ
// refreshRate =  0xe, result.refreshHz = 0x42f00000(120.00)
// refreshRate =  0xf, result.refreshHz = 0x43480000(200.00)
// refreshRate = 0x10, result.refreshHz = 0x436fc28f(239.76)
// refreshRate = 0x11, result.refreshHz = 0x43700000(240.00)
// refreshRate = 0x14, result.refreshHz = 0x413fd70a( 11.99)
// refreshRate = 0x15, result.refreshHz = 0x41400000( 12.00)
// refreshRate = 0x16, result.refreshHz = 0x416fd70a( 14.99)
// refreshRate = 0x17, result.refreshHz = 0x41700000( 15.00)
// refreshRate = 0x23, result.refreshHz = 0x42b3d1ec( 89.91) REFRESH_RATE_89_91HZ
// clang-format on

struct DceFile : public orbis::File {};

struct DceDevice : IoDevice {
  orbis::shared_mutex mtx;
  orbis::uint64_t dmemOffset = ~static_cast<std::uint64_t>(0);
  VideoOutBuffer bufferAttributes{}; // TODO
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static void initDceMemory(DceDevice *device) {
  if (device->dmemOffset + 1) {
    return;
  }

  std::lock_guard lock(device->mtx);
  if (device->dmemOffset + 1) {
    return;
  }

  auto dmem = orbis::g_context.dmemDevice.cast<DmemDevice>();
  std::uint64_t start = 0;
  if (dmem->allocate(&start, ~0ull, kDceControlMemorySize, 0x100000, 1) !=
      orbis::ErrorCode{}) {
    std::abort();
  }

  void *address = nullptr;
  if (dmem->mmap(&address, kDceControlMemorySize, rx::vm::kMapProtCpuWrite, 0,
                 start) != orbis::ErrorCode{}) {
    std::abort();
  }

  auto dceControl = reinterpret_cast<std::byte *>(address);
  *reinterpret_cast<orbis::uint64_t *>(dceControl + 0x130) = 0;
  *reinterpret_cast<orbis::uint64_t *>(dceControl + 0x138) = 1;
  *reinterpret_cast<orbis::uint16_t *>(dceControl + 0x140) =
      orbis::kEvFiltDisplay;
  rx::vm::unmap(address, kDceControlMemorySize);
  device->dmemOffset = start;
}

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

    // ORBIS_LOG_NOTICE("dce: FlipControl", args->id, args->arg2, args->ptr,
    //                  args->size);

    if (args->id == 0) {
      ORBIS_LOG_NOTICE("dce: FlipControl: open",
                        args->padding, args->arg2, args->ptr, args->size,
                        args->arg5, args->arg6);
      ORBIS_RET_ON_ERROR(
          orbis::uwrite(orbis::ptr<orbis::ulong>(args->arg5 + 0), 1));
      return orbis::uwrite(orbis::ptr<orbis::sint>(args->arg5 + 8), 0);
    }

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
      flipStatus.flipPendingNum0 = 0; // TODO
      flipStatus.gcQueueNum = 0;      // TODO
      flipStatus.flipPendingNum1 = 0; // TODO
      flipStatus.submitTsc = 0;       // TODO

      std::memcpy(args->ptr, &flipStatus, sizeof(FlipControlStatus));
    } else if (args->id == 12) {
      *(std::uint64_t *)args->ptr = 0;
    } else if (args->id == 18) {
      // ORBIS_LOG_NOTICE("dce: get vblank status", args->size);
      if (args->size) {
        *(std::uint32_t *)args->size = 0xffff'0000;
      }
    } else if (args->id == 19) {
      // get resolution status
      auto status = (ResolutionStatus *)args->ptr;
      status->width = 1920;
      status->heigth = 1080;
      status->paneWidth = 1920;
      status->paneHeight = 1080;
      status->refreshHz = 0x426fc28f;        //( 59.94)
      status->screenSizeInInch = 0x42500000; //( 52.00)
    } else if (args->id == 9) {
      ORBIS_LOG_NOTICE("dce: FlipControl allocate", args->id, args->arg2,
                       args->ptr, args->size);
      *(std::uint64_t *)args->ptr = 0;         // dev offset
      *(std::uint64_t *)args->size = 0x100000; // size
    } else if (args->id == 31) {
      // if ((std::uint64_t)args->ptr == 0xc) {
      //   rx::bridge.header->bufferInUseAddress = args->size;
      // } else {
      //   ORBIS_LOG_ERROR("buffer in use", args->ptr, args->size);
      //   thread->where();
      // }
      // std::abort();
      return {};
    } else if (args->id == 33) { // adjust color
      std::printf("adjust color\n");
      return {};
    } else if (args->id == 0x1e) {
      // TODO
      return {};
    } else if (args->id != 1) { // used during open/close
      ORBIS_LOG_NOTICE("dce: UNIMPLEMENTED FlipControl", args->id, args->arg2,
                       args->ptr, args->size);

      thread->where();
      std::fflush(stdout);
      //__builtin_trap();
    }
    return {};
  }

  if (request == 0xc0308206) {
    auto args = reinterpret_cast<RegisterBuffer *>(argp);
    ORBIS_LOG_ERROR("dce: RegisterBuffer", args->canary, args->index,
                    args->address, args->address2);

    if (args->index >= std::size(rx::bridge.header->buffers)) {
      // TODO
      ORBIS_LOG_FATAL("dce: out of buffers!", args->index);
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

    ORBIS_LOG_ERROR("dce: RegisterBufferAttributes", args->canary, args->attrid,
                    args->submit, args->unk3, args->pixelFormat,
                    args->tilingMode, args->pitch, args->width, args->height,
                    args->unk4_zero, args->unk5_zero, args->options,
                    args->reserved1, args->reserved2);

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

    // ORBIS_LOG_ERROR("dce: FlipRequestArgs", args->canary,
    //                 args->displayBufferIndex, args->flipMode, args->unk1,
    //                 args->flipArg, args->flipArg2, args->eop_nz, args->unk2,
    //                 args->eop_val, args->unk3, args->unk4, args->rout);

    rx::bridge.sendFlip(thread->tproc->pid, args->displayBufferIndex,
                        /*args->flipMode,*/ args->flipArg);

    // *args->rout = 0;

    // rx::bridge.header->flipBuffer = args->displayBufferIndex;
    // rx::bridge.header->flipArg = args->flipArg;
    // rx::bridge.header->flipCount += 1;
    // *reinterpret_cast<std::uint64_t *>(rx::bridge.header->bufferInUseAddress)
    // =
    //     0;
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
  auto dce = file->device.cast<DceDevice>();
  initDceMemory(dce.get());
  auto dmem = orbis::g_context.dmemDevice.cast<DmemDevice>();
  return dmem->mmap(address, size, prot, flags, dce->dmemOffset + offset);
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
