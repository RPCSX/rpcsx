#include "dmem.hpp"
#include "gpu/DeviceCtl.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/align.hpp"
#include "rx/format.hpp"
#include "rx/watchdog.hpp"
#include "vm.hpp"
#include <fcntl.h>
#include <filesystem>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>

struct DmemFile : public orbis::File {};

struct AllocateDirectMemoryArgs {
  std::uint64_t searchStart;
  std::uint64_t searchEnd;
  std::uint64_t len;
  std::uint64_t alignment;
  std::uint32_t memoryType;
};

static constexpr auto dmemSize = 0x5000000000;
// static const std::uint64_t nextOffset = 0;
//  static const std::uint64_t memBeginAddress = 0xfe0000000;

DmemDevice::~DmemDevice() {
  if (shmFd > 0) {
    close(shmFd);
  }

  std::filesystem::remove(rx::format("{}/dmem-{}", rx::getShmPath(), index));
}

orbis::ErrorCode DmemDevice::mmap(void **address, std::uint64_t len,
                                  std::int32_t prot, std::int32_t flags,
                                  std::int64_t directMemoryStart) {
  if (prot == 0) {
    // hack
    // fixme: implement protect for pid
    prot = vm::kMapProtCpuReadWrite | vm::kMapProtGpuAll;
  }

  if (*address == nullptr) {
    *address = std::bit_cast<void *>(0x80000000ull);
    flags &= ~vm::kMapFlagFixed;
  }

  int memoryType = 0;
  if (auto allocationInfoIt = allocations.queryArea(directMemoryStart);
      allocationInfoIt != allocations.end()) {
    memoryType = allocationInfoIt->memoryType;
  }

  auto result = vm::map(*address, len, prot, flags, vm::kMapInternalReserveOnly,
                        this, directMemoryStart);

  ORBIS_LOG_WARNING("dmem mmap", index, directMemoryStart, len, prot, flags,
                    result, *address);
  if (result == (void *)-1) {
    return orbis::ErrorCode::NOMEM; // TODO
  }

  if (::mmap(result, len, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, shmFd,
             directMemoryStart) == (void *)-1) {
    return orbis::ErrorCode::INVAL;
  }

  if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
    gpu.submitMapMemory(orbis::g_currentThread->tproc->pid,
                        reinterpret_cast<std::uint64_t>(result), len,
                        memoryType, index, prot, directMemoryStart);
  }

  *address = result;

  return {};
}

static orbis::ErrorCode dmem_ioctl(orbis::File *file, std::uint64_t request,
                                   void *argp, orbis::Thread *thread) {
  auto device = static_cast<DmemDevice *>(file->device.get());

  std::lock_guard lock(device->mtx);
  switch (request) {
  case 0x4008800a: // get size
    ORBIS_LOG_WARNING("dmem getTotalSize", device->index, argp);
    *(std::uint64_t *)argp = device->dmemTotalSize / 0x10;
    return {};

  case 0xc0208016: { // get available size
    struct Args {
      std::uint64_t searchStart;
      std::uint64_t searchEnd;
      std::uint64_t alignment;
      std::uint64_t size;
    };

    auto args = reinterpret_cast<Args *>(argp);

    return device->queryMaxFreeChunkSize(&args->searchStart, args->searchEnd,
                                         args->alignment, &args->size);
  }

  case 0xc0288010: // sceKernelAllocateDirectMemoryForMiniApp
  case 0xc0288011:
  case 0xc0288001: { // sceKernelAllocateDirectMemory
    auto args = reinterpret_cast<AllocateDirectMemoryArgs *>(argp);

    return device->allocate(&args->searchStart, args->searchEnd, args->len,
                            args->alignment, args->memoryType);
  }

  case 0xc018800d: { // transfer budget
    return {};
  }

  case 0xc018800f: { // protect memory for pid
    struct Args {
      std::uint64_t address;
      std::uint64_t size;
      std::uint32_t pid; // 0 if all
      std::uint32_t prot;
    };
    return {};
  }

  case 0x80108015:   // sceKernelCheckedReleaseDirectMemory
  case 0x80108002: { // sceKernelReleaseDirectMemory
    struct Args {
      std::uint64_t address;
      std::uint64_t size;
    };

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_WARNING("dmem releaseDirectMemory", device->index, args->address,
                      args->size);

    device->allocations.map(args->address, args->address + args->size,
                            {.memoryType = -1u});
    return {};
  }

  case 0xc0208004: { // get direct memory type
    struct Args {
      std::uint64_t start;
      std::uint64_t regionStart;
      std::uint64_t regionEnd;
      std::uint32_t memoryType;
    };

    auto args = reinterpret_cast<Args *>(argp);

    auto it = device->allocations.lowerBound(args->start);

    if (it == device->allocations.end() || it->memoryType == -1u) {
      return orbis::ErrorCode::SRCH;
    }

    args->regionStart = it.beginAddress();
    args->regionEnd = it.endAddress();
    args->memoryType = it->memoryType;
    return {};
  }

  case 0x80288012: { // direct memory query
    struct DirectMemoryQueryInfo {
      std::uint64_t start;
      std::uint64_t end;
      std::uint32_t memoryType;
    };

    struct Args {
      std::uint32_t devIndex;
      std::uint32_t flags;
      std::uint32_t unk;
      std::uint64_t offset;
      orbis::ptr<DirectMemoryQueryInfo> info;
      std::uint64_t infoSize;
    };

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_WARNING("dmem directMemoryQuery", device->index, args->devIndex,
                      args->unk, args->flags, args->offset, args->info,
                      args->infoSize);

    if (args->devIndex != device->index) {
      // TODO
      ORBIS_LOG_ERROR("dmem directMemoryQuery: device mismatch", device->index,
                      args->devIndex, args->unk, args->flags, args->offset,
                      args->info, args->infoSize);

      return orbis::ErrorCode::INVAL;
    }

    if (args->infoSize != sizeof(DirectMemoryQueryInfo)) {
      return orbis::ErrorCode::INVAL;
    }

    auto it = device->allocations.lowerBound(args->offset);

    if (it == device->allocations.end()) {
      return orbis::ErrorCode::ACCES;
    }

    auto queryInfo = *it;

    if (it->memoryType == -1u) {
      return orbis::ErrorCode::ACCES;
    }

    if ((args->flags & 1) == 0) {
      if (it.endAddress() <= args->offset) {
        return orbis::ErrorCode::ACCES;
      }
    } else {
      if (it.beginAddress() > args->offset || it.endAddress() <= args->offset) {
        return orbis::ErrorCode::ACCES;
      }
    }

    DirectMemoryQueryInfo info{
        .start = it.beginAddress(),
        .end = it.endAddress(),
        .memoryType = it->memoryType,
    };

    ORBIS_LOG_WARNING("dmem directMemoryQuery", device->index, args->devIndex,
                      args->unk, args->flags, args->offset, args->info,
                      args->infoSize, info.start, info.end, info.memoryType);
    return orbis::uwrite(args->info, info);
  }
  }

  ORBIS_LOG_FATAL("Unhandled dmem ioctl", device->index, request);
  thread->where();
  return {};
}

static orbis::ErrorCode dmem_mmap(orbis::File *file, void **address,
                                  std::uint64_t size, std::int32_t prot,
                                  std::int32_t flags, std::int64_t offset,
                                  orbis::Thread *thread) {
  auto device = static_cast<DmemDevice *>(file->device.get());
  return device->mmap(address, size, prot, flags, offset);
}

static const orbis::FileOps ops = {
    .ioctl = dmem_ioctl,
    .mmap = dmem_mmap,
};

orbis::ErrorCode DmemDevice::allocate(std::uint64_t *start,
                                      std::uint64_t searchEnd,
                                      std::uint64_t len,
                                      std::uint64_t alignment,
                                      std::uint32_t memoryType) {
  std::size_t offset = *start;
  if (alignment == 0) {
    alignment = 1;
  }
  if (searchEnd == 0) {
    searchEnd = dmemTotalSize;
  }

  while (offset < searchEnd) {
    offset += alignment - 1;
    offset &= ~(alignment - 1);

    if (offset + len > dmemTotalSize) {
      ORBIS_LOG_ERROR("dmem: failed to allocate direct memory: out of memory",
                      *start, searchEnd, len, alignment, memoryType, offset);
      return orbis::ErrorCode::AGAIN;
    }

    auto it = allocations.lowerBound(offset);

    if (it != allocations.end()) {
      if (it->memoryType == -1u) {
        if (offset < it.beginAddress()) {
          offset = it.beginAddress() + alignment - 1;
          offset &= ~(alignment - 1);
        }

        if (offset + len >= it.endAddress()) {
          offset = it.endAddress();
          continue;
        }
      } else {
        if (offset + len > it.beginAddress()) {
          offset = it.endAddress();
          continue;
        }
      }
    }

    allocations.map(offset, offset + len,
                    {
                        .memoryType = memoryType,
                    });
    ORBIS_LOG_WARNING("dmem: allocated direct memory", *start, searchEnd, len,
                      alignment, memoryType, offset);
    *start = offset;
    return {};
  }

  ORBIS_LOG_ERROR("dmem: failed to allocate direct memory", *start, searchEnd,
                  len, alignment, memoryType, offset);
  return orbis::ErrorCode::AGAIN;
}

orbis::ErrorCode DmemDevice::release(std::uint64_t start, std::uint64_t size) {
  allocations.unmap(start, start + size);
  return {};
}

orbis::ErrorCode DmemDevice::queryMaxFreeChunkSize(std::uint64_t *start,
                                                   std::uint64_t searchEnd,
                                                   std::uint64_t alignment,
                                                   std::uint64_t *size) {
  std::size_t offset = *start;
  std::size_t resultSize = 0;
  std::size_t resultOffset = 0;

  alignment = std::max(alignment, vm::kPageSize);
  alignment = rx::alignUp(alignment, vm::kPageSize);

  while (offset < searchEnd) {
    offset += alignment - 1;
    offset &= ~(alignment - 1);

    if (offset >= dmemTotalSize) {
      break;
    }

    auto it = allocations.lowerBound(offset);

    if (it == allocations.end()) {
      if (resultSize < dmemTotalSize - offset) {
        resultSize = dmemTotalSize - offset;
        resultOffset = offset;
      }

      break;
    }

    if (it->memoryType == -1u) {
      if (offset < it.beginAddress()) {
        offset = it.beginAddress() + alignment - 1;
        offset &= ~(alignment - 1);
      }

      if (it.endAddress() > offset && resultSize < it.endAddress() - offset) {
        resultSize = it.endAddress() - offset;
        resultOffset = offset;
      }
    } else if (offset > it.beginAddress() &&
               resultSize < offset - it.beginAddress()) {
      resultSize = offset - it.beginAddress();
      resultOffset = offset;
    }

    offset = it.endAddress();
  }

  resultSize /= 0x20;

  *start = resultOffset;
  *size = resultSize;

  ORBIS_LOG_WARNING("dmem queryMaxFreeChunkSize", resultOffset, resultSize);

  if (resultSize == 0) {
    return orbis::ErrorCode::NOMEM;
  }
  return {};
}

orbis::ErrorCode DmemDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                  std::uint32_t flags, std::uint32_t mode,
                                  orbis::Thread *thread) {
  auto newFile = orbis::knew<DmemFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createDmemCharacterDevice(int index) {
  auto *newDevice = orbis::knew<DmemDevice>();
  newDevice->index = index;
  newDevice->dmemTotalSize = dmemSize;

  auto path = rx::format("{}/dmem-{}", rx::getShmPath(), index);
  auto shmFd = ::open(path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  if (ftruncate(shmFd, dmemSize) < 0) {
    ::close(shmFd);
    std::abort();
  }

  newDevice->shmFd = shmFd;
  return newDevice;
}
