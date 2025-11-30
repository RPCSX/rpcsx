#pragma once

#include "MemoryType.hpp"
#include "error/ErrorCode.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include "vmem.hpp"
#include <utility>

namespace orbis {
struct Process;
struct IoDevice;
} // namespace orbis

namespace orbis::dmem {
static constexpr auto kPageSize = 64 * 1024;

enum class QueryFlags {
  LowerBound,
  Pooled,

  bitset_last = Pooled,
};

struct QueryResult {
  rx::AddressRange range;
  MemoryType memoryType;
};

ErrorCode initialize();
ErrorCode clear(unsigned dmemIndex);

std::pair<std::uint64_t, ErrorCode>
allocate(unsigned dmemIndex, rx::AddressRange searchRange, std::uint64_t len,
         MemoryType memoryType, std::uint64_t alignment = kPageSize,
         bool pooled = false);

ErrorCode release(unsigned dmemIndex, rx::AddressRange range,
                  bool pooled = false);

std::pair<std::uint64_t, ErrorCode> reserveSystem(unsigned dmemIndex,
                                                  std::uint64_t size);
std::pair<std::uint64_t, ErrorCode> reservePooled(unsigned dmemIndex,
                                                  rx::AddressRange searchRange,
                                                  std::uint64_t size,
                                                  std::uint64_t alignment);

ErrorCode setType(unsigned dmemIndex, rx::AddressRange range, MemoryType type,
                  rx::EnumBitSet<QueryFlags> flags = {});
std::optional<QueryResult> query(unsigned dmemIndex, std::uint64_t dmemOffset,
                                 rx::EnumBitSet<QueryFlags> flags = {});
std::uint64_t getSize(unsigned dmemIndex);
std::pair<rx::AddressRange, ErrorCode>
getAvailSize(unsigned dmemIndex, rx::AddressRange searchRange,
             std::uint64_t alignment);

ErrorCode map(unsigned dmemIndex, rx::AddressRange range, std::uint64_t offset,
              rx::EnumBitSet<vmem::Protection> protection);

std::pair<std::uint64_t, ErrorCode> getPmemOffset(unsigned dmemIndex,
                                                  std::uint64_t dmemOffset);
} // namespace orbis::dmem
