#pragma once

#include "MemoryType.hpp"
#include "kernel/MemoryResource.hpp"
#include "orbis-config.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include "rx/StaticString.hpp"
#include "rx/mem.hpp"
#include <string_view>

namespace orbis {
using kernel::AllocationFlags;

struct File;
struct Process;
} // namespace orbis

namespace orbis::vmem {
static constexpr auto kPageSize = 16 * 1024;

enum class Protection {
  CpuRead,
  CpuWrite,
  CpuExec,
  GpuRead = 4,
  GpuWrite,

  bitset_last = GpuWrite
};

enum class BlockFlags : std::uint8_t {
  FlexibleMemory,
  DirectMemory,
  Stack,
  PooledMemory,
  Commited,

  bitset_last = Commited
};

enum class BlockFlagsEx : std::uint8_t {
  Allocated,
  Private,
  Shared,
  PoolControl,
  Reserved,

  bitset_last = Reserved
};

enum class MapFlags {
  Shared = 0,
  Private = 1,
  Fixed = 4,
  Rename = 5,
  NoReserve = 6,
  NoOverwrite = 7,
  Void = 8,
  HasSemaphore = 9,
  Stack = 10,
  NoSync = 11,
  Anon = 12,
  System = 13,
  AllAvailable = 14,
  NoCore = 17,
  PrefaultRead = 18,
  Self = 19,
  NoCoalesce = 22,

  bitset_last = NoCoalesce
};

inline constexpr std::uint32_t kMapFlagsAlignShift = 24;
inline constexpr std::uint32_t kMapFlagsAlignMask = 0x1f << kMapFlagsAlignShift;

inline constexpr auto kProtCpuReadWrite =
    Protection::CpuRead | Protection::CpuWrite;
inline constexpr auto kProtCpuAll =
    Protection::CpuRead | Protection::CpuWrite | Protection::CpuExec;
inline constexpr auto kProtGpuAll = Protection::GpuRead | Protection::GpuWrite;

inline std::pair<std::uint64_t, rx::EnumBitSet<MapFlags>>
unpackMapFlags(rx::EnumBitSet<MapFlags> flags, std::uint64_t minAlignment) {
  std::uint64_t alignment = minAlignment;

  if (auto align =
          (flags.toUnderlying() & kMapFlagsAlignMask) >> kMapFlagsAlignShift) {
    alignment = std::uint64_t(1) << align;
    flags = rx::EnumBitSet<vmem::MapFlags>::fromUnderlying(
        flags.toUnderlying() & ~kMapFlagsAlignMask);

    if (alignment < minAlignment) {
      alignment = minAlignment;
    }
  }

  return {alignment, flags};
}

#pragma pack(push, 1)
struct QueryResult {
  uint64_t start;
  uint64_t end;
  uint64_t offset;
  rx::EnumBitSet<Protection> protection;
  MemoryType memoryType;
  rx::EnumBitSet<BlockFlags> flags;
  rx::StaticCString<32> name;
  char _padding[7];
};

static_assert(sizeof(QueryResult) == 72);

struct MemoryProtection {
  uint64_t startAddress;
  uint64_t endAddress;
  rx::EnumBitSet<Protection> prot;
  uint32_t _padding;
};

static_assert(sizeof(MemoryProtection) == 24);
#pragma pack(pop)

inline constexpr rx::EnumBitSet<rx::mem::Protection>
toCpuProtection(rx::EnumBitSet<Protection> prot) {
  rx::EnumBitSet<rx::mem::Protection> result{};
  if (prot & Protection::CpuRead) {
    result |= rx::mem::Protection::R;
  }
  if (prot & Protection::CpuWrite) {
    result |= rx::mem::Protection::W;
  }
  if (prot & Protection::CpuExec) {
    result |= rx::mem::Protection::X;
  }
  return result;
}

inline constexpr rx::EnumBitSet<rx::mem::Protection>
toGpuProtection(rx::EnumBitSet<Protection> prot) {
  rx::EnumBitSet<rx::mem::Protection> result{};
  if (prot & Protection::GpuRead) {
    result |= rx::mem::Protection::R;
  }
  if (prot & Protection::GpuWrite) {
    result |= rx::mem::Protection::W;
  }
  return result;
}

void initialize(Process *process, bool force = false);
void fork(Process *process, Process *parentThread);

std::pair<rx::AddressRange, ErrorCode>
reserve(Process *process, std::uint64_t addressHint, std::uint64_t size,
        rx::EnumBitSet<AllocationFlags> allocFlags,
        rx::EnumBitSet<BlockFlagsEx> blockFlagsEx = {},
        std::uint64_t alignment = kPageSize);

std::pair<rx::AddressRange, ErrorCode>
mapFile(Process *process, std::uint64_t addressHint, std::uint64_t size,
        rx::EnumBitSet<AllocationFlags> allocFlags,
        rx::EnumBitSet<Protection> prot, rx::EnumBitSet<BlockFlags> blockFlags,
        rx::EnumBitSet<BlockFlagsEx> blockFlagsEx, File *file,
        std::uint64_t fileOffset, std::string_view name = {},
        std::uint64_t alignment = kPageSize,
        MemoryType type = MemoryType::Invalid);

std::pair<rx::AddressRange, ErrorCode>
mapDirect(Process *process, std::uint64_t addressHint,
          rx::AddressRange directRange, rx::EnumBitSet<Protection> prot,
          rx::EnumBitSet<AllocationFlags> allocFlags,
          std::string_view name = {}, std::uint64_t alignment = kPageSize,
          MemoryType type = MemoryType::Invalid);

std::pair<rx::AddressRange, ErrorCode>
mapFlex(Process *process, std::uint64_t size, rx::EnumBitSet<Protection> prot,
        std::uint64_t addressHint = 0,
        rx::EnumBitSet<AllocationFlags> allocFlags = {},
        rx::EnumBitSet<BlockFlags> blockFlags = {}, std::string_view name = {},
        std::uint64_t alignment = kPageSize);

std::pair<rx::AddressRange, ErrorCode>
commitPooled(Process *process, rx::AddressRange addressRange, MemoryType type,
             rx::EnumBitSet<Protection> prot);
ErrorCode decommitPooled(Process *process, rx::AddressRange addressRange);

ErrorCode protect(Process *process, rx::AddressRange range,
                  rx::EnumBitSet<Protection> prot);
ErrorCode unmap(Process *process, rx::AddressRange range);
ErrorCode setName(Process *process, rx::AddressRange range,
                  std::string_view name);
ErrorCode setType(Process *process, rx::AddressRange range, MemoryType type);
ErrorCode setTypeAndProtect(Process *process, rx::AddressRange range,
                            MemoryType type, rx::EnumBitSet<Protection> prot);
std::optional<QueryResult> query(Process *process, std::uint64_t address,
                                 bool lowerBound = false);
std::optional<MemoryProtection> queryProtection(Process *process,
                                                std::uint64_t address,
                                                bool lowerBound = false);
} // namespace orbis::vmem
