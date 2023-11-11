#include "bridge.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "vm.hpp"
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

struct GcDevice : public IoDevice {
  orbis::shared_mutex mtx;
  orbis::kmap<std::uint64_t, ComputeQueue> computeQueues;
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct GcFile : public orbis::File {};
static std::uint64_t g_submitDoneFlag;

static orbis::ErrorCode gc_ioctl(orbis::File *file, std::uint64_t request,
                                 void *argp, orbis::Thread *thread) {
  // 0xc00c8110
  // 0xc0848119

  auto device = static_cast<GcDevice *>(file->device.get());
  std::lock_guard lock(device->mtx);

  switch (request) {
  case 0xc008811b: // get submit done flag ptr?
                   // TODO
    ORBIS_LOG_ERROR("gc ioctl 0xc008811b", *(std::uint64_t *)argp);
    *reinterpret_cast<void **>(argp) = &g_submitDoneFlag;
    break;

  case 0xc0108102: { // submit?
    struct Args {
      std::uint32_t arg0;
      std::uint32_t count;
      std::uint64_t *cmds;
    };

    auto args = reinterpret_cast<Args *>(argp);

    flockfile(stderr);
    ORBIS_LOG_ERROR("gc ioctl 0xc0108102", args->arg0, args->count, args->cmds);

    for (unsigned i = 0; i < args->count; ++i) {
      auto cmd = args->cmds + (i * 2);
      auto cmdId = cmd[0] & 0xffff'ffff;
      auto addressLoPart = cmd[0] >> 32;
      auto addressHiPart = cmd[1] & 0xff;
      auto address = addressLoPart | (addressHiPart << 32);
      auto unkPreservedVal = cmd[1] & 0xfff00000ffffff00;
      auto size = ((cmd[1] >> 32) & 0xfffff) << 2;

      // std::fprintf(stderr, "     %lx\n", cmd[0]);
      // std::fprintf(stderr, "     %lx\n", cmd[1]);
      // std::fprintf(stderr, "  %u:\n", i);
      // std::fprintf(stderr, "    cmdId = %lx\n", cmdId);
      // std::fprintf(stderr, "    address = %lx\n", address);
      // std::fprintf(stderr, "    unkPreservedVal = %lx\n", unkPreservedVal);
      // std::fprintf(stderr, "    size = %lu\n", size);

      rx::bridge.sendCommandBuffer(thread->tproc->pid, cmdId, address, size);
    }
    funlockfile(stderr);

    break;
  }

  case 0xc0088101: { // switch buffer?
    struct Args {
      std::uint32_t arg0;
      std::uint32_t arg1;
    };

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_ERROR("gc ioctl 0xc0088101", args->arg0, args->arg1);
    break;
  }

  case 0xc020810c: { // submit and flip?
    struct Args {
      std::uint32_t arg0;
      std::uint32_t count;
      std::uint64_t *cmds;
      std::uint64_t arg3; // flipArg?
      std::uint32_t arg4; // bufferIndex?
    };

    auto args = reinterpret_cast<Args *>(argp);

    flockfile(stderr);
    ORBIS_LOG_ERROR("gc ioctl 0xc020810c", args->arg0, args->count, args->cmds,
                    args->arg3, args->arg4);

    for (unsigned i = 0; i < args->count; ++i) {
      auto cmd = args->cmds + (i * 2);
      auto cmdId = cmd[0] & 0xffff'ffff;
      auto addressLoPart = cmd[0] >> 32;
      auto addressHiPart = cmd[1] & 0xff;
      auto address = addressLoPart | (addressHiPart << 32);
      auto unkPreservedVal = cmd[1] & 0xfff00000ffffff00;
      auto size = ((cmd[1] >> 32) & 0xfffff) << 2;

      // std::fprintf(stderr, "     %lx\n", cmd[0]);
      // std::fprintf(stderr, "     %lx\n", cmd[1]);
      // std::fprintf(stderr, "  %u:\n", i);
      // std::fprintf(stderr, "    cmdId = %lx\n", cmdId);
      // std::fprintf(stderr, "    address = %lx\n", address);
      // std::fprintf(stderr, "    unkPreservedVal = %lx\n", unkPreservedVal);
      // std::fprintf(stderr, "    size = %lu\n", size);

      rx::bridge.sendCommandBuffer(thread->tproc->pid, cmdId, address, size);
    }
    funlockfile(stderr);

    // orbis::bridge.sendDoFlip();
    break;
  }

  case 0xc0048116: {
    ORBIS_LOG_ERROR("gc ioctl 0xc0048116", *(std::uint32_t *)argp);
    thread->where();
    break;
  }

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

  case 0xc010810b: { // something like stats masks?
    struct Args {
      std::uint64_t arg1;
      std::uint64_t arg2;
    };

    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_ERROR("gc ioctl stats mask", args->arg1, args->arg2);
    break;
  }

  case 0xc030810d: { // map compute queue
    struct Args {
      std::uint32_t pipeHi;
      std::uint32_t pipeLo;
      std::uint32_t queueId;
      std::uint32_t offset;
      std::uint64_t ringBaseAddress;
      std::uint64_t readPtrAddress;
      std::uint64_t dingDongPtr;
      std::uint32_t lenLog2;
    };

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_ERROR("gc ioctl map compute queue", args->pipeHi, args->pipeLo,
                    args->queueId, args->offset, args->ringBaseAddress,
                    args->readPtrAddress, args->dingDongPtr, args->lenLog2);

    auto id = ((args->pipeHi * 4) + args->pipeLo) * 8 + args->queueId;
    device->computeQueues[id] = {
        .ringBaseAddress = args->ringBaseAddress,
        .readPtrAddress = args->readPtrAddress,
        .dingDongPtr = args->dingDongPtr,
        .len = static_cast<std::uint64_t>(1) << args->lenLog2,
    };
    args->pipeHi = 0x769c766;
    args->pipeLo = 0x72e8e3c1;
    args->queueId = -0x248d50d8;
    args->offset = 0xd245ed58;

    ((std::uint64_t *)args->dingDongPtr)[0xf0 / sizeof(std::uint64_t)] = 1;
    break;
  }

  case 0xc010811c: {
    // ding dong for workload
    struct Args {
      std::uint32_t pipeHi;
      std::uint32_t pipeLo;
      std::uint32_t queueId;
      std::uint32_t nextStartOffsetInDw;
    };

    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_ERROR("gc ioctl ding dong for workload", args->pipeHi,
                    args->pipeLo, args->queueId, args->nextStartOffsetInDw);

    auto id = ((args->pipeHi * 4) + args->pipeLo) * 8 + args->queueId;

    auto queue = device->computeQueues.at(id);
    auto address = (queue.ringBaseAddress + queue.offset);
    auto endOffset = static_cast<std::uint64_t>(args->nextStartOffsetInDw) << 2;
    auto size = endOffset - queue.offset;

    rx::bridge.sendCommandBuffer(thread->tproc->pid, id, address, size);

    queue.offset = endOffset;
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
    std::fprintf(stderr, "***WARNING*** Unknown gc ioctl_%lx(0x%lx)\n", request,
                 (unsigned long)*(std::uint32_t *)argp);
    break;
  }

  default:
    ORBIS_LOG_FATAL("Unhandled gc ioctl", request);
    std::fflush(stdout);
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
  auto result = rx::vm::map(*address, size, prot, flags);

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
  *file = newFile;
  return {};
}

IoDevice *createGcCharacterDevice() { return orbis::knew<GcDevice>(); }
