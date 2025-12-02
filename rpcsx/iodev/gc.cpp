#include "dce.hpp"
#include "gpu/DeviceCtl.hpp"
#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/dmem.hpp"
#include "orbis/file.hpp"
#include "orbis/pmem.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/vmem.hpp"
#include "rx/AddressRange.hpp"
#include "rx/SharedMutex.hpp"
#include "rx/die.hpp"
#include "rx/format.hpp"
#include "rx/print.hpp"
#include "rx/watchdog.hpp"
#include <cstdio>
#include <mutex>
#include <sys/mman.h>

struct ComputeQueue {
  std::uint64_t ringBaseAddress{};
  std::uint64_t readPtrAddress{};
  std::uint64_t dingDongPtr{};
  std::uint64_t offset{};
  std::uint64_t len{};
};

struct GcDevice : public orbis::IoDevice {
  rx::shared_mutex mtx;
  rx::AddressRange dmemRange;
  orbis::kmap<orbis::pid_t, int> clients;
  orbis::kmap<std::uint64_t, ComputeQueue> computeQueues;
  orbis::uintptr_t submitArea = 0;
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  void addClient(orbis::Process *process);
  void removeClient(orbis::Process *process);

  GcDevice() { blockFlags = orbis::vmem::BlockFlags::DirectMemory; }

  ~GcDevice() { orbis::pmem::deallocate(dmemRange); }

  orbis::ErrorCode map(rx::AddressRange range, std::int64_t offset,
                       rx::EnumBitSet<orbis::vmem::Protection> protection,
                       orbis::File *file, orbis::Process *process) override {
    if (offset + range.size() > dmemRange.size()) {
      return orbis::ErrorCode::INVAL;
    }

    rx::println(stderr, "map gc {:x}-{:x} {:04x} {}", range.beginAddress(),
                range.endAddress(), offset, protection);

    auto result =
        orbis::pmem::map(range.beginAddress(),
                         rx::AddressRange::fromBeginSize(
                             dmemRange.beginAddress() + offset, range.size()),
                         orbis::vmem::toCpuProtection(protection));

    if (result == orbis::ErrorCode{}) {
      amdgpu::mapMemory(process->pid, range, orbis::MemoryType::WbOnion,
                        protection, dmemRange.beginAddress() + offset);
    }

    return result;
  }
};

struct GcFile : public orbis::File {
  orbis::Process *process = nullptr;
  int gfxPipe = 0;

  ~GcFile() { device.rawStaticCast<GcDevice>()->removeClient(process); }
};

static orbis::ErrorCode gc_ioctl(orbis::File *file, std::uint64_t request,
                                 void *argp, orbis::Thread *thread) {
  // 0xc00c8110
  // 0xc0848119

  auto gcFile = static_cast<GcFile *>(file);
  auto device = file->device.rawStaticCast<GcDevice>();
  std::lock_guard lock(device->mtx);

  switch (request) {
  case 0xc008811b: // get submit done flag ptr?
    if (device->submitArea == 0) {
      auto [dmemOffset, dmemErrc] = orbis::dmem::allocate(
          0, rx::AddressRange::fromBeginEnd(0, 0), orbis::dmem::kPageSize,
          orbis::MemoryType::WcGarlic);

      if (dmemErrc != orbis::ErrorCode{}) {
        return dmemErrc;
      }

      auto directRange =
          rx::AddressRange::fromBeginSize(dmemOffset, orbis::dmem::kPageSize);

      auto [vmemRange, vmemErrc] = orbis::vmem::mapDirect(
          thread->tproc, 0xfe0100000, directRange,
          orbis::vmem::Protection::CpuRead | orbis::vmem::Protection::CpuWrite |
              orbis::vmem::Protection::GpuRead |
              orbis::vmem::Protection::GpuWrite,
          {});

      if (vmemErrc != orbis::ErrorCode{}) {
        orbis::dmem::release(0, directRange);
        return vmemErrc;
      }

      device->submitArea = vmemRange.beginAddress();
    }

    ORBIS_LOG_ERROR("gc ioctl 0xc008811b", *(std::uint64_t *)argp);
    *reinterpret_cast<orbis::uintptr_t *>(argp) = device->submitArea;
    break;

  case 0xc004812e: {
    if (orbis::g_context->fwType != orbis::FwType::Ps5) {
      return orbis::ErrorCode::INVAL;
    }

    struct Args {
      std::uint16_t unk;
      std::uint16_t flag;
    };

    auto args = reinterpret_cast<Args *>(argp);
    args->unk = 0;
    args->flag = 0;
    return {};
  }

  case 0xc0488131: {
    if (orbis::g_context->fwType != orbis::FwType::Ps5) {
      return orbis::ErrorCode::INVAL;
    }

    // ps5 submit header
    struct Args {
      orbis::uint32_t unk0;
      orbis::uint32_t contextControl[3];
      orbis::uint32_t cmds[3 * 4];
      orbis::uint64_t status;
    };

    static_assert(sizeof(Args) == 72);

    auto args = reinterpret_cast<Args *>(argp);

    // ORBIS_LOG_ERROR("gc submit header", args->status, args->unk0,
    //                 args->contextControl[0], args->contextControl[1],
    //                 args->contextControl[2]);

    // for (std::size_t i = 0; i < 3; ++i) {
    //   auto offset = i * 4;
    //   ORBIS_LOG_ERROR("gc submit header cmd", i, args->cmds[offset],
    //                   args->cmds[offset + 1], args->cmds[offset + 2],
    //                   args->cmds[offset + 3]);
    // }

    // thread->where();

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      if (args->contextControl[0]) {
        gpu.submitGfxCommand(gcFile->gfxPipe,
                             orbis::g_currentThread->tproc->vmId,
                             {args->contextControl, 3});
      }

      for (std::size_t i = 0; i < 3; ++i) {
        auto offset = i * 4;

        if (args->cmds[offset] == 0) {
          continue;
        }

        ORBIS_LOG_ERROR("submit header", i, args->status, args->cmds[offset],
                        args->cmds[offset + 1], args->cmds[offset + 2],
                        args->cmds[offset + 3]);
        gpu.submitGfxCommand(gcFile->gfxPipe,
                             orbis::g_currentThread->tproc->vmId,
                             {args->cmds + offset, 4});
      }

      gpu.waitForIdle();
      args->status = 0;
    } else {
      return orbis::ErrorCode::BUSY;
    }

    return {};
  }

  case 0xc0188132: {
    if (orbis::g_context->fwType != orbis::FwType::Ps5) {
      return orbis::ErrorCode::INVAL;
    }

    struct Submit {
      std::uint64_t unk0;
      std::uint64_t unk1;
      std::uint64_t address;
      std::uint64_t size;
    };

    struct Args {
      orbis::uint32_t unk0; // ringId?
      orbis::uint32_t count;
      orbis::ptr<Submit> submits;
      orbis::uint32_t status;
      orbis::uint32_t padding;
    };

    static_assert(sizeof(Args) == 24);
    auto args = reinterpret_cast<Args *>(argp);

    // ORBIS_LOG_ERROR("gc submit", args->unk0, args->count, args->submits,
    //                 args->status, args->padding);

    // for (std::size_t i = 0; i < args->count / 2; ++i) {
    //   ORBIS_LOG_ERROR("gc submit cmd", i, args->submits[i].address,
    //                   args->submits[i].size, args->submits[i].unk0,
    //                   args->submits[i].unk1);
    // }

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      for (unsigned i = 0; i < args->count / 2; ++i) {
        auto addressLo = static_cast<std::uint32_t>(args->submits[i].address);
        auto addressHi =
            static_cast<std::uint32_t>(args->submits[i].address >> 32);
        auto size = static_cast<std::uint32_t>(args->submits[i].size);

        gpu.submitGfxCommand(gcFile->gfxPipe,
                             orbis::g_currentThread->tproc->vmId,
                             {{0xc0023f00, addressLo, addressHi, size}});
      }

      gpu.waitForIdle();
    } else {
      return orbis::ErrorCode::BUSY;
    }

    args->status = 0;
    return {};
  }

  case 0xc0108102: { // submit?
    struct Args {
      orbis::uint32_t arg0;
      orbis::uint32_t count;
      orbis::uint32_t *cmds;
    };

    auto args = reinterpret_cast<Args *>(argp);
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      for (unsigned i = 0; i < args->count; ++i) {
        gpu.submitGfxCommand(gcFile->gfxPipe,
                             orbis::g_currentThread->tproc->vmId,
                             {args->cmds + i * 4, 4});
      }

      gpu.waitForIdle();
    } else {
      return orbis::ErrorCode::BUSY;
    }
    break;
  }

  case 0xc0088101: { // switch buffer?
    struct Args {
      std::uint32_t arg0;
      std::uint32_t arg1;
    };

    auto args = reinterpret_cast<Args *>(argp);
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      gpu.waitForIdle();
      gpu.submitSwitchBuffer(orbis::g_currentThread->tproc->gfxRing);
    } else {
      return orbis::ErrorCode::BUSY;
    }

    // ORBIS_LOG_ERROR("gc ioctl 0xc0088101", args->arg0, args->arg1);
    break;
  }

  case 0xc020810c: { // submit and write eop
    struct Args {
      orbis::uint32_t arg0;
      orbis::uint32_t count;
      orbis::ptr<orbis::uint32_t> cmds;
      orbis::uint64_t eopValue;
      orbis::uint32_t waitFlag;
    };

    auto args = reinterpret_cast<Args *>(argp);

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      for (unsigned i = 0; i < args->count; ++i) {
        gpu.submitGfxCommand(gcFile->gfxPipe,
                             orbis::g_currentThread->tproc->vmId,
                             {args->cmds + i * 4, 4});
      }

      // ORBIS_LOG_ERROR("submit and write eop", args->eopValue,
      // args->waitFlag);
      gpu.submitWriteEop(gcFile->gfxPipe, args->waitFlag, args->eopValue);
      gpu.waitForIdle();
    } else {
      return orbis::ErrorCode::BUSY;
    }

    break;
  }

  case 0xc0048116: { // submit done?
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      // gpu.waitForIdle();
    } else {
      return orbis::ErrorCode::BUSY;
    }
  }

  case 0xc0048117:
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      gpu.waitForIdle();
    } else {
      return orbis::ErrorCode::BUSY;
    }
    break;

  case 0xc00c8110: {
    // set gs ring sizes
    struct Args {
      std::uint32_t arg1;
      std::uint32_t arg2;
      std::uint32_t unk; // 0
    };
    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_ERROR("gc ioctl set gs ring sizes", args->arg1, args->arg2,
                    args->unk);
    break;
  }

  case 0xc0848119: { // stats report control?
    struct Args {
      std::uint32_t unk; // 0x10001
      std::uint32_t arg1;
      std::uint32_t arg2;
      std::uint32_t arg3;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_ERROR("gc ioctl stats report control", args->unk, args->arg1,
                    args->arg2, args->arg3);
    break;
  }

  case 0xc010810b: { // get cu masks param
    struct Args {
      std::uint32_t se0sh0;
      std::uint32_t se0sh1;
      std::uint32_t se1sh0;
      std::uint32_t se1sh1;
    };

    auto args = reinterpret_cast<Args *>(argp);
    // ORBIS_LOG_ERROR("gc ioctl stats mask", args->arg1, args->arg2);
    args->se0sh0 = ~0;
    args->se0sh1 = ~0;
    args->se1sh0 = ~0;
    args->se1sh1 = ~0;
    break;
  }

  case 0xc030810d: { // map compute queue
    struct Args {
      orbis::uint32_t meId;
      orbis::uint32_t pipeId;
      orbis::uint32_t queueId;
      orbis::uint32_t vqueueId;
      orbis::uintptr_t ringBaseAddress;
      orbis::uintptr_t readPtrAddress;
      orbis::uintptr_t doorbell;
      orbis::uint32_t ringSize;
    };

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_ERROR("gc ioctl map compute queue", args->meId, args->pipeId,
                    args->queueId, args->vqueueId, args->ringBaseAddress,
                    args->readPtrAddress, args->doorbell, args->ringSize);

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      gpu.mapComputeQueue(thread->tproc->vmId, args->meId, args->pipeId,
                          args->queueId, args->vqueueId, args->ringBaseAddress,
                          args->readPtrAddress, args->doorbell,
                          static_cast<std::uint64_t>(1) << args->ringSize);

    } else {
      return orbis::ErrorCode::BUSY;
    }
    break;
  }

  case 0xc010811c: {
    // ding dong for workload
    struct Args {
      std::uint32_t meId;
      std::uint32_t pipeId;
      std::uint32_t queueId;
      std::uint32_t nextStartOffsetInDw;
    };

    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_ERROR("gc ioctl ding dong for workload", args->meId, args->pipeId,
                    args->queueId, args->nextStartOffsetInDw);

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context->gpuDevice}) {
      gpu.submitComputeQueue(args->meId, args->pipeId, args->queueId,
                             args->nextStartOffsetInDw);
      gpu.waitForIdle();
    } else {
      return orbis::ErrorCode::BUSY;
    }
    break;
  }

  case 0xc0048114: {
    // SetWaveLimitMultipliers
    ORBIS_LOG_WARNING("Unknown gc ioctl", request,
                      (unsigned long)*(std::uint32_t *)argp);
    thread->where();
    break;
  }

  case 0xc004811f: {
    ORBIS_LOG_WARNING("Unknown gc ioctl", request,
                      (unsigned long)*(std::uint32_t *)argp);
    *(std::uint32_t *)argp = 0;
    break;
  }

  case 0x802450c9: {
    // used during Net initialization
    rx::println(stderr, "***WARNING*** Unknown gc ioctl_{:x}(0x{:x})", request,
                (unsigned long)*(std::uint32_t *)argp);
    break;
  }

  case 0xc0048113: {
    // get num clients

    struct Args {
      std::uint32_t numClients;
    };

    auto *args = reinterpret_cast<Args *>(argp);
    args->numClients = device->clients.size();
    break;
  }

  case 0xc0048115: {
    // is game closed
    *(std::uint32_t *)argp = 0;
    break;
  }

  default:
    ORBIS_LOG_FATAL("Unhandled gc ioctl", request);
    std::fflush(stdout);
    thread->where();
    // __builtin_trap();
    break;
  }
  return {};
}

static const orbis::FileOps ops = {.ioctl = gc_ioctl};

static void createGpu() {
  {
    std::lock_guard lock(orbis::g_context->gpuDeviceMtx);
    if (orbis::g_context->gpuDevice != nullptr) {
      return;
    }

    rx::createGpuDevice();
  }

  while (orbis::g_context->gpuDevice == nullptr) {
    std::this_thread::yield();
  }
}

orbis::ErrorCode GcDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                std::uint32_t flags, std::uint32_t mode,
                                orbis::Thread *thread) {
  createGpu();

  auto newFile = orbis::knew<GcFile>();
  newFile->device = this;
  newFile->ops = &ops;
  newFile->process = thread->tproc;
  addClient(thread->tproc);
  *file = newFile;
  return {};
}

void GcDevice::addClient(orbis::Process *process) {
  auto dce = orbis::g_context->dceDevice.rawStaticCast<DceDevice>();
  dce->initializeProcess(process);

  std::lock_guard lock(mtx);
  auto &client = clients[process->pid];
  ++client;
}

void GcDevice::removeClient(orbis::Process *process) {
  std::lock_guard lock(mtx);
  auto clientIt = clients.find(process->pid);
  assert(clientIt != clients.end());
  assert(clientIt->second != 0);
  --clientIt->second;
  if (clientIt->second == 0) {
    clients.erase(clientIt);
  }
}

orbis::IoDevice *createGcCharacterDevice() {
  auto result = orbis::knew<GcDevice>();
  auto dmemSize = orbis::dmem::getSize(0);
  auto [dmemOffset, errc] = orbis::dmem::allocate(
      0,
      rx::AddressRange::fromBeginEnd(dmemSize - orbis::dmem::kPageSize * 2,
                                     dmemSize),
      orbis::dmem::kPageSize, orbis::MemoryType::WcGarlic);

  rx::dieIf(errc != orbis::ErrorCode{},
            "failed to allocate GC memory, error {}", errc);
  result->dmemRange =
      rx::AddressRange::fromBeginSize(dmemOffset, orbis::dmem::kPageSize);
  return result;
}
