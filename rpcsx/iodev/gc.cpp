#include "gpu/DeviceCtl.hpp"
#include "io-device.hpp"
#include "iodev/dce.hpp"
#include "iodev/dmem.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "rx/die.hpp"
#include "vm.hpp"
#include <cstdio>
#include <mutex>
#include <print>
#include <sys/mman.h>

struct ComputeQueue {
  std::uint64_t ringBaseAddress{};
  std::uint64_t readPtrAddress{};
  std::uint64_t dingDongPtr{};
  std::uint64_t offset{};
  std::uint64_t len{};
};

struct GcDevice : public IoDevice {
  orbis::shared_mutex mtx;
  orbis::kmap<orbis::pid_t, int> clients;
  orbis::kmap<std::uint64_t, ComputeQueue> computeQueues;
  void *submitArea = nullptr;
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  void addClient(orbis::Process *process);
  void removeClient(orbis::Process *process);
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
  // std::lock_guard lock(device->mtx);

  switch (request) {
  case 0xc008811b: // get submit done flag ptr?
    if (device->submitArea == nullptr) {
      auto dmem = orbis::g_context.dmemDevice.staticCast<DmemDevice>();
      std::uint64_t start = 0;
      auto err = dmem->allocate(&start, ~0, vm::kPageSize, 0, 0);
      if (err != orbis::ErrorCode{}) {
        return err;
      }
      auto address = reinterpret_cast<void *>(0xfe0100000);
      err = dmem->mmap(&address, vm::kPageSize,
                       vm::kMapProtCpuReadWrite | vm::kMapProtGpuAll,
                       vm::kMapFlagShared, start);
      if (err != orbis::ErrorCode{}) {
        dmem->release(start, vm::kPageSize);
        return err;
      }
      device->submitArea = address;
    }

    ORBIS_LOG_ERROR("gc ioctl 0xc008811b", *(std::uint64_t *)argp);
    *reinterpret_cast<void **>(argp) = device->submitArea;
    break;

  case 0xc0108102: { // submit?
    struct Args {
      orbis::uint32_t arg0;
      orbis::uint32_t count;
      orbis::uint32_t *cmds;
    };

    auto args = reinterpret_cast<Args *>(argp);
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      for (unsigned i = 0; i < args->count; ++i) {
        gpu.submitGfxCommand(gcFile->gfxPipe,
                             orbis::g_currentThread->tproc->vmId,
                             {args->cmds + i * 4, 4});
      }

      // gpu.waitForIdle();
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
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      gpu.waitForIdle();
      gpu.submitSwitchBuffer(orbis::g_currentThread->tproc->vmId);
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

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      for (unsigned i = 0; i < args->count; ++i) {
        gpu.submitGfxCommand(gcFile->gfxPipe,
                             orbis::g_currentThread->tproc->vmId,
                             {args->cmds + i * 4, 4});
      }

      // ORBIS_LOG_ERROR("submit and write eop", args->eopValue,
      // args->waitFlag);
      gpu.submitWriteEop(gcFile->gfxPipe, args->waitFlag, args->eopValue);
    } else {
      return orbis::ErrorCode::BUSY;
    }

    break;
  }

  case 0xc0048116: { // submit done?
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      gpu.waitForIdle();
    } else {
      return orbis::ErrorCode::BUSY;
    }
  }

  case 0xc0048117:
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
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

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
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

    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      gpu.submitComputeQueue(args->meId, args->pipeId, args->queueId,
                             args->nextStartOffsetInDw);
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
    break;
  }

  case 0x802450c9: {
    // used during Net initialization
    std::println(stderr, "***WARNING*** Unknown gc ioctl_{:x}(0x{:x})", request,
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

static orbis::ErrorCode gc_mmap(orbis::File *file, void **address,
                                std::uint64_t size, std::int32_t prot,
                                std::int32_t flags, std::int64_t offset,
                                orbis::Thread *thread) {
  ORBIS_LOG_FATAL("gc mmap", address, size, offset);
  auto result = vm::map(*address, size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = gc_ioctl,
    .mmap = gc_mmap,
};

orbis::ErrorCode GcDevice::open(orbis::Ref<orbis::File> *file, const char *path,
                                std::uint32_t flags, std::uint32_t mode,
                                orbis::Thread *thread) {
  auto newFile = orbis::knew<GcFile>();
  newFile->device = this;
  newFile->ops = &ops;
  newFile->process = thread->tproc;
  addClient(thread->tproc);
  *file = newFile;
  return {};
}

void GcDevice::addClient(orbis::Process *process) {
  auto dce = orbis::g_context.dceDevice.rawStaticCast<DceDevice>();
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

IoDevice *createGcCharacterDevice() { return orbis::knew<GcDevice>(); }
