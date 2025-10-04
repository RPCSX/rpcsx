#include "dce.hpp"
#include "gpu/DeviceCtl.hpp"
#include "io-device.hpp"
#include "iodev/dmem.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/die.hpp"
#include "rx/mem.hpp"
#include "rx/watchdog.hpp"
#include "vm.hpp"
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sys/mman.h>

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

struct FlipControlStatus2 {
  uint64_t count;
  uint64_t processTime;
  uint64_t reserved0;
  int64_t flipArg;
  uint64_t unk0;
  uint64_t processTimeCounter;
  int32_t gcQueueNum;
  int32_t flipPendingNum;
  int32_t currentBuffer;
  uint32_t unk1;
  uint64_t submitProcessTimeCounter;
  uint64_t unk2[7];
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
  float refreshHz;        // float
  float screenSizeInInch; // float
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

static void runBridge(int vmId) {
  std::thread{[=] {
    pthread_setname_np(pthread_self(), "Bridge");

    auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice};
    auto &gpuCtx = gpu.getContext();
    std::vector<std::uint64_t> fetchedCommands;
    fetchedCommands.reserve(std::size(gpuCtx.cpuCacheCommands));

    std::vector<std::atomic<std::uint64_t> *> fetchedAtomics;
    std::uint32_t prevIdleValue = 0;

    while (true) {
      if (gpuCtx.cpuCacheCommandsIdle[vmId].wait(prevIdleValue) !=
          std::errc{}) {
        continue;
      }

      prevIdleValue =
          gpuCtx.cpuCacheCommandsIdle[vmId].load(std::memory_order::acquire);

      for (auto &command : gpuCtx.cpuCacheCommands[vmId]) {
        std::uint64_t value = command.load(std::memory_order::relaxed);

        if (value != 0) {
          fetchedCommands.push_back(value);
          fetchedAtomics.push_back(&command);
        }
      }

      if (fetchedCommands.empty()) {
        continue;
      }

      for (auto command : fetchedCommands) {
        auto page = static_cast<std::uint32_t>(command);
        auto count = static_cast<std::uint32_t>(command >> 32) + 1;

        auto pageFlags =
            gpuCtx.cachePages[vmId][page].load(std::memory_order::relaxed);

        auto address = static_cast<std::uint64_t>(page) * rx::mem::pageSize;
        auto origVmProt = vm::getPageProtection(address);
        int prot = 0;

        if (origVmProt & vm::kMapProtCpuRead) {
          prot |= PROT_READ;
        }
        if (origVmProt & vm::kMapProtCpuWrite) {
          prot |= PROT_WRITE;
        }
        if (origVmProt & vm::kMapProtCpuExec) {
          prot |= PROT_EXEC;
        }

        if (pageFlags & amdgpu::kPageReadWriteLock) {
          prot &= ~(PROT_READ | PROT_WRITE);
        } else if (pageFlags & amdgpu::kPageWriteWatch) {
          prot &= ~PROT_WRITE;
        }

        if (::mprotect(reinterpret_cast<void *>(address),
                       rx::mem::pageSize * count, prot)) {
          perror("protection failed");
          std::abort();
        }
      }

      for (auto fetchedAtomic : fetchedAtomics) {
        fetchedAtomic->store(0, std::memory_order::release);
      }

      fetchedCommands.clear();
      fetchedAtomics.clear();
    }
  }}.detach();
}

struct DceFile : public orbis::File {};

int DceDevice::allocateVmId() {
  int id = std::countr_zero(freeVmIds);

  if (id >= kVmIdCount) {
    rx::die("out of vm slots");
  }

  freeVmIds &= ~(1 << id);
  return id;
}

void DceDevice::deallocateVmId(int vmId) { freeVmIds |= (1 << vmId); }

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
  if (dmem->mmap(&address, kDceControlMemorySize, vm::kMapProtCpuWrite, 0,
                 start) != orbis::ErrorCode{}) {
    std::abort();
  }

  auto dceControl = reinterpret_cast<std::byte *>(address);
  *reinterpret_cast<orbis::uint64_t *>(dceControl + 0x130) = 0;
  *reinterpret_cast<orbis::uint64_t *>(dceControl + 0x138) = 1;
  *reinterpret_cast<orbis::uint16_t *>(dceControl + 0x140) =
      orbis::kEvFiltDisplay;
  vm::unmap(address, kDceControlMemorySize);
  device->dmemOffset = start;
}

static orbis::ErrorCode dce_mmap(orbis::File *file, void **address,
                                 std::uint64_t size, std::int32_t prot,
                                 std::int32_t flags, std::int64_t offset,
                                 orbis::Thread *thread);

static orbis::ErrorCode dce_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {
  auto device = static_cast<DceDevice *>(file->device.get());

  auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice};
  auto &gpuCtx = gpu.getContext();

  // std::this_thread::sleep_for(std::chrono::seconds(5));

  if (orbis::g_context.fwType == orbis::FwType::Ps5) {
    if (request == 0x80308217) {
      auto args = reinterpret_cast<FlipControlArgs *>(argp);

      if (args->id == 0x36) {
        auto flip = (FlipRequestArgs *)args->ptr;

        // ORBIS_LOG_ERROR("dce: flip2", flip->canary, flip->displayBufferIndex,
        //                 flip->flipMode, flip->unk1, flip->flipArg,
        //                 flip->flipArg2, flip->eop_nz, flip->unk2,
        //                 flip->eop_val, flip->unk3, flip->rout);

        if (flip->eop_nz == 0) {
          gpu.submitFlip(thread->tproc->pid, flip->displayBufferIndex,
                         flip->flipArg);
        } else if (flip->eop_nz == 1) {
          std::uint64_t eopValue = flip->canary;
          eopValue ^= 0xff00'0000;
          eopValue ^= static_cast<std::uint64_t>(device->eopCount++) << 32;

          ORBIS_RET_ON_ERROR(gpu.submitFlipOnEop(
              thread->tproc->gfxRing, thread->tproc->pid,
              flip->displayBufferIndex, flip->flipArg, eopValue));
          *flip->eop_val = eopValue;
        }
        return {};
      }

      if (args->id == 9) {
        ORBIS_LOG_NOTICE("dce: FlipControl allocate", args->id, args->padding,
                         args->arg2, args->ptr, args->size, args->arg5,
                         args->arg6);

        void *address;
        ORBIS_RET_ON_ERROR(
            dce_mmap(file, &address, vm::kPageSize,
                     vm::kMapProtCpuReadWrite | vm::kMapProtGpuAll,
                     vm::kMapFlagShared, 0, thread));

        *(void **)args->ptr = address;
        *(std::uint64_t *)args->arg5 = vm::kPageSize;

        return {};
      }
      if (args->id == 0x38) {
        auto attrs = (RegisterBufferAttributeArgs *)args->ptr;

        ORBIS_LOG_ERROR("dce: RegisterBufferAttributes2", attrs->canary,
                        (int)attrs->attrid, (int)attrs->submit, attrs->unk3,
                        attrs->pixelFormat, attrs->tilingMode, attrs->pitch,
                        attrs->width, attrs->height, (int)attrs->unk4_zero,
                        (int)attrs->unk5_zero, attrs->options, attrs->reserved1,
                        attrs->reserved2);

        gpu.registerBufferAttribute(thread->tproc->pid,
                                    {
                                        .attrId = attrs->attrid,
                                        .submit = attrs->submit,
                                        .canary = attrs->canary,
                                        .pixelFormat = attrs->pixelFormat,
                                        .tilingMode = attrs->tilingMode,
                                        .pitch = attrs->pitch,
                                        .width = attrs->width,
                                        .height = attrs->height,
                                    });

        return {};
      }

      if (args->id == 0x37) {
        auto buffer = (RegisterBuffer *)args->ptr;
        ORBIS_LOG_ERROR("dce: RegisterBuffer2", buffer->address,
                        buffer->address2, buffer->canary, buffer->index,
                        buffer->attrid, args->id, args->padding, args->arg2,
                        args->ptr, args->size, args->arg5, args->arg6);
        gpu.registerBuffer(thread->tproc->pid,
                           {
                               .canary = buffer->canary,
                               .index = uint32_t(args->size),
                               .attrId = buffer->attrid,
                               .address = buffer->address,
                               .address2 = buffer->address2,
                           });
        return {};
      }

      if (args->id == 0x33) {
        if (args->arg2 == 0 && args->ptr == nullptr && args->arg5 == 0 &&
            args->arg6 == 0) {
          gpuCtx.bufferInUseAddress[thread->tproc->vmId] = args->size;
          return {};
        }
      }

      if (args->id == 0xa) {
        FlipControlStatus2 flipStatus{};
        flipStatus.flipArg = gpuCtx.flipArg[thread->tproc->vmId];
        flipStatus.count = gpuCtx.flipCount[thread->tproc->vmId];
        flipStatus.currentBuffer = gpuCtx.flipBuffer[thread->tproc->vmId];
        // TODO

        std::memcpy(args->ptr, &flipStatus, sizeof(FlipControlStatus2));
        return {};
      }

      ORBIS_LOG_FATAL("dce: unimplemented 0x80308217 request", args->id,
                      args->padding, args->arg2, args->ptr, args->size,
                      args->arg5, args->arg6);
      return {};
    }
  }

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
      ORBIS_LOG_NOTICE("dce: FlipControl: open", args->padding, args->arg2,
                       args->ptr, args->size, args->arg5, args->arg6);
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
      flipStatus.flipArg = gpuCtx.flipArg[thread->tproc->vmId];
      flipStatus.count = gpuCtx.flipCount[thread->tproc->vmId];
      flipStatus.processTime = 0; // TODO
      flipStatus.tsc = 0;         // TODO
      flipStatus.currentBuffer = gpuCtx.flipBuffer[thread->tproc->vmId];
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
      status->refreshHz = 59.94f;
      status->screenSizeInInch = 52.0f;
    } else if (args->id == 9) {
      ORBIS_LOG_NOTICE("dce: FlipControl allocate", args->id, args->arg2,
                       args->ptr, args->size);
      *(std::uint64_t *)args->ptr = kDceControlMemoryOffset; // dev offset
      *(std::uint64_t *)args->size = kDceControlMemorySize;  // size
    } else if (args->id == 31) {
      if ((std::uint64_t)args->ptr == 0xc) {
        gpuCtx.bufferInUseAddress[thread->tproc->vmId] = args->size;
      } else if ((std::uint64_t)args->ptr != 1) {
        ORBIS_LOG_ERROR("buffer in use", args->ptr, args->size);
        thread->where();
      }
      // std::abort();
      return {};
    } else if (args->id == 33) { // adjust color
      std::printf("adjust color\n");
      return {};
    } else if (args->id == 0x1e) {
      // TODO
      return {};
    } else if (args->id == 1) {

      // Mode set
      orbis::g_context.deviceEventEmitter->emit(
          orbis::kEvFiltDisplay,
          [](orbis::KNote *note) -> std::optional<orbis::intptr_t> {
            if ((note->event.ident >> 48) == 0x64) {
              return 0;
            }
            return {};
          });

    } else { // used during open/close
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

    gpu.registerBuffer(thread->tproc->pid, {
                                               .canary = args->canary,
                                               .index = args->index,
                                               .attrId = args->attrid,
                                               .address = args->address,
                                               .address2 = args->address2,
                                           });
    return {};
  }

  if (request == 0xc0308207) { // SCE_SYS_DCE_IOCTL_REGISTER_BUFFER_ATTRIBUTE
    auto args = reinterpret_cast<RegisterBufferAttributeArgs *>(argp);

    ORBIS_LOG_ERROR("dce: RegisterBufferAttributes", args->canary, args->attrid,
                    args->submit, args->unk3, args->pixelFormat,
                    args->tilingMode, args->pitch, args->width, args->height,
                    args->unk4_zero, args->unk5_zero, args->options,
                    args->reserved1, args->reserved2);

    gpu.registerBufferAttribute(thread->tproc->pid,
                                {
                                    .attrId = args->attrid,
                                    .submit = args->submit,
                                    .canary = args->canary,
                                    .pixelFormat = args->pixelFormat,
                                    .tilingMode = args->tilingMode,
                                    .pitch = args->pitch,
                                    .width = args->width,
                                    .height = args->height,
                                });

    return {};
  }

  if (request == 0xc0488204) {
    // flip request
    auto args = reinterpret_cast<FlipRequestArgs *>(argp);

    if (args->eop_nz == 0) {
      gpu.submitFlip(thread->tproc->pid, args->displayBufferIndex,
                     args->flipArg);
    } else if (args->eop_nz == 1) {
      std::uint64_t eopValue = args->canary;
      eopValue ^= 0xff00'0000;
      eopValue ^= static_cast<std::uint64_t>(device->eopCount++) << 32;

      ORBIS_RET_ON_ERROR(gpu.submitFlipOnEop(
          thread->tproc->gfxRing, thread->tproc->pid, args->displayBufferIndex,
          args->flipArg, eopValue));
      *args->eop_val = eopValue;
    }

    *args->rout = 0;
    return {};
  }

  if (request == 0x80308217) {
    ORBIS_LOG_ERROR(__FUNCTION__, request);
    thread->where();
    return {};
  }

  if (request == 0x80208218) {
    ORBIS_LOG_ERROR(__FUNCTION__, request);
    thread->where();
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

static void createGpu() {
  {
    std::lock_guard lock(orbis::g_context.gpuDeviceMtx);
    if (orbis::g_context.gpuDevice != nullptr) {
      return;
    }

    rx::createGpuDevice();
  }

  while (orbis::g_context.gpuDevice == nullptr) {
  }
}

orbis::ErrorCode DceDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                 std::uint32_t flags, std::uint32_t mode,
                                 orbis::Thread *thread) {
  auto newFile = orbis::knew<DceFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  initializeProcess(thread->tproc);
  return {};
}

void DceDevice::initializeProcess(orbis::Process *process) {
  if (process->vmId == -1) {
    createGpu();
    auto vmId = allocateVmId();

    std::lock_guard lock(orbis::g_context.gpuDeviceMtx);
    {
      auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice};
      gpu.submitMapProcess(process->pid, vmId);
      process->vmId = vmId;
    }

    runBridge(vmId);
  }
}

IoDevice *createDceCharacterDevice() { return orbis::knew<DceDevice>(); }
