#pragma once

#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Rc.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include <cstdint>
#include <rx/MemoryTable.hpp>
#include <unistd.h>

struct DmemDevice : public IoDevice {
  orbis::shared_mutex mtx;
  int index;
  int shmFd = -1;
  std::size_t dmemTotalSize;

  ~DmemDevice();

  struct AllocationInfo {
    std::uint32_t memoryType;

    bool operator==(const AllocationInfo &) const = default;
    auto operator<=>(const AllocationInfo &) const = default;
  };

  rx::MemoryTableWithPayload<AllocationInfo, orbis::kallocator> allocations;

  orbis::ErrorCode allocate(std::uint64_t *start, std::uint64_t searchEnd,
                            std::uint64_t len, std::uint64_t alignment,
                            std::uint32_t memoryType);

  orbis::ErrorCode queryMaxFreeChunkSize(std::uint64_t *start,
                                         std::uint64_t searchEnd,
                                         std::uint64_t alignment,
                                         std::uint64_t *size);

  orbis::ErrorCode release(std::uint64_t start, std::uint64_t size);

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  orbis::ErrorCode mmap(void **address, std::uint64_t len, std::int32_t prot,
                        std::int32_t flags, std::int64_t directMemoryStart);
};
