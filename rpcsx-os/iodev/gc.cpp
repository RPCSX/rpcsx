#include <sys/mman.h>

#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "rpcsx-os/vm.hpp"
#include "rpcsx-os/bridge.hpp"
#include "rpcsx-os/io-device.hpp"
#include "orbis/KernelAllocator.hpp"
// #include <rpcs4/bridge.hpp>

struct GcDevice : public IoDevice {};

struct GcInstance : public IoDeviceInstance {};

static std::uint64_t g_submitDoneFlag;

static std::int64_t gc_instance_ioctl(IoDeviceInstance *instance,
                                      std::uint64_t request, void *argp) {
  // 0xc00c8110
  // 0xc0848119

  switch (request) {
  case 0xc008811b: // get submit done flag ptr?
    // TODO
    std::fprintf(stderr, "gc ioctl 0xc008811b(%lx)\n", *(std::uint64_t *)argp);
    *reinterpret_cast<void **>(argp) = &g_submitDoneFlag;
    return 0;

  case 0xc0108102: { // submit?
    struct Args {
      std::uint32_t arg0;
      std::uint32_t count;
      std::uint64_t *cmds;
    };

    auto args = reinterpret_cast<Args *>(argp);

    std::fprintf(stderr, "gc ioctl 0xc0108102(%x, %x, %p)\n", args->arg0,
                 args->count, args->cmds);

    for (int i = 0; i < args->count; ++i) {
      auto cmd = args->cmds + (i * 2);
      auto cmdId = cmd[0] & 0xffff'ffff;
      auto addressLoPart = cmd[0] >> 32;
      auto addressHiPart = cmd[1] & 0xff;
      auto address = addressLoPart | (addressHiPart << 32);
      auto unkPreservedVal = cmd[1] & 0xfff00000ffffff00;
      auto size = ((cmd[1] >> 32) & 0xfffff) << 2;

      // std::fprintf(stderr, "     %lx\n", cmd[0]);
      // std::fprintf(stderr, "     %lx\n", cmd[1]);
      std::fprintf(stderr, "  %u:\n", i);
      std::fprintf(stderr, "    cmdId = %lx\n", cmdId);
      std::fprintf(stderr, "    address = %lx\n", address);
      std::fprintf(stderr, "    unkPreservedVal = %lx\n", unkPreservedVal);
      std::fprintf(stderr, "    size = %lu\n", size);

      rx::bridge.sendCommandBuffer(cmdId, address, size);
    }

    break;
  }

  case 0xc0088101: { // switch buffer?
    struct Args {
      std::uint32_t arg0;
      std::uint32_t arg1;
    };

    auto args = reinterpret_cast<Args *>(argp);

    std::fprintf(stderr, "gc ioctl 0xc0088101(%x, %x)\n", args->arg0,
                 args->arg1);
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

    std::fprintf(stderr, "gc ioctl 0xc020810c(%x, %x, %p, %lx, %x)\n",
                 args->arg0, args->count, args->cmds, args->arg3, args->arg4);

    for (int i = 0; i < args->count; ++i) {
      auto cmd = args->cmds + (i * 2);
      auto cmdId = cmd[0] & 0xffff'ffff;
      auto addressLoPart = cmd[0] >> 32;
      auto addressHiPart = cmd[1] & 0xff;
      auto address = addressLoPart | (addressHiPart << 32);
      auto unkPreservedVal = cmd[1] & 0xfff00000ffffff00;
      auto size = ((cmd[1] >> 32) & 0xfffff) << 2;

      // std::fprintf(stderr, "     %lx\n", cmd[0]);
      // std::fprintf(stderr, "     %lx\n", cmd[1]);
      std::fprintf(stderr, "  %u:\n", i);
      std::fprintf(stderr, "    cmdId = %lx\n", cmdId);
      std::fprintf(stderr, "    address = %lx\n", address);
      std::fprintf(stderr, "    unkPreservedVal = %lx\n", unkPreservedVal);
      std::fprintf(stderr, "    size = %lu\n", size);

      rx::bridge.sendCommandBuffer(cmdId, address, size);
    }

    //orbis::bridge.sendDoFlip();
    break;
  }

  case 0xc0048116: {
    std::fprintf(stderr, "gc ioctl 0xc0048116(%x)\n", *(std::uint32_t *)argp);
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

    std::fprintf(stderr,
                 "gc ioctl set gs ring sizes: arg1=0x%x, arg2=0x%x, unk=0x%x\n",
                 args->arg1, args->arg2, args->unk);
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
    std::fprintf(
      stderr,
      "gc ioctl stats report control(unk=%x,arg1=%x,arg2=%x,arg3=%x)\n",
      args->unk, args->arg1, args->arg2, args->arg3);
    break;
  }

  case 0xc010810b: { // something like stats masks?
    struct Args {
      std::uint64_t arg1;
      std::uint64_t arg2;
    };

    auto args = reinterpret_cast<Args *>(argp);
    std::fprintf(stderr, "gc ioctl stats mask(arg1=%lx,arg2=%lx)\n", args->arg1,
                 args->arg2);
    break;
  }

  case 0xc030810d: { // map compute queue
    struct Args {
      std::uint32_t pipeHi;
      std::uint32_t pipeLo;
      std::uint32_t queueId;
      std::uint32_t queuePipe;
      std::uint64_t ringBaseAddress;
      std::uint64_t readPtrAddress;
      std::uint64_t dingDongPtr;
      std::uint32_t count;
    };

    auto args = reinterpret_cast<Args *>(argp);

    std::fprintf(stderr,
                 "gc ioctl map compute queue(pipeHi=%x, pipeLo=%x, queueId=%x, "
                 "queuePipe=%x, ringBaseAddress=%lx, readPtrAddress=%lx, "
                 "unkPtr=%lx, count=%u)\n",
                 args->pipeHi, args->pipeLo, args->queueId, args->queuePipe,
                 args->ringBaseAddress, args->readPtrAddress, args->dingDongPtr,
                 args->count);

    args->pipeHi = 0x769c766;
    args->pipeLo = 0x72e8e3c1;
    args->queueId = -0x248d50d8;
    args->queuePipe = 0xd245ed58;

    ((std::uint64_t *)args->dingDongPtr)[0xf0 / sizeof(std::uint64_t)] = 1;

    // TODO: implement
    // std::fflush(stdout);
    //__builtin_trap();
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
    std::fprintf(stderr,
                 "gc ioctl ding dong for workload(pipeHi=%x, pipeLo=%x, queueId=%x, "
                 "nextStartOffsetInDw=%x)\n",
                 args->pipeHi, args->pipeLo, args->queueId, args->nextStartOffsetInDw);


    // TODO: implement

    break;
  }

  case 0xc0048114: {
    // SetWaveLimitMultipliers
    std::fprintf(stderr, "***WARNING*** Unknown gc ioctl_%lx(0x%lx)\n", request, (unsigned long)*(std::uint32_t *)argp);
    break;
  }

  case 0xc004811f: {
    std::fprintf(stderr, "***WARNING*** Unknown gc ioctl_%lx(0x%lx)\n", request, (unsigned long)*(std::uint32_t *)argp);
    break;
  }

  default:
    std::fprintf(stderr, "***ERROR*** Unhandled gc ioctl %lx\n", request);
    std::fflush(stdout);
    __builtin_trap();
    break;
  }
  return 0;
}

static void *gc_instance_mmap(IoDeviceInstance *instance, void *address,
                              std::uint64_t size, std::int32_t prot,
                              std::int32_t flags, std::int64_t offset) {
  std::fprintf(stderr, "***ERROR*** Unhandled gc mmap %lx\n", offset);

  return rx::vm::map(address, size, prot, flags);
}

static std::int32_t gc_device_open(IoDevice *device,
                                   orbis::Ref<IoDeviceInstance> *instance,
                                   const char *path, std::uint32_t flags,
                                   std::uint32_t mode) {
  auto *newInstance = orbis::knew<GcInstance>();
  newInstance->ioctl = gc_instance_ioctl;
  newInstance->mmap = gc_instance_mmap;
  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createGcCharacterDevice() {
  auto *newDevice = orbis::knew<GcDevice>();
  newDevice->open = gc_device_open;
  return newDevice;
}
