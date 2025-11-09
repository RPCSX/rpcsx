#pragma once

#include "kernel/MemoryResource.hpp"
#include "orbis-config.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include "rx/StaticString.hpp"
#include <string_view>

namespace orbis {
using kernel::AllocationFlags;
}

namespace orbis {
struct IoDevice;
struct Process;

namespace vmem {
static constexpr auto kPageSize = 16 * 1024;

enum class Protection {
  CpuRead,
  CpuWrite,
  CpuExec,
  GpuRead = 4,
  GpuWrite,

  bitset_last = GpuWrite
};

enum class BlockFlags {
  FlexibleMemory,
  DirectMemory,
  Stack,
  PooledMemory,
  Commited,
  Allocated,

  bitset_last = Allocated
};

inline constexpr auto kProtCpuReadWrite =
    Protection::CpuRead | Protection::CpuWrite;
inline constexpr auto kProtCpuAll =
    Protection::CpuRead | Protection::CpuWrite | Protection::CpuExec;
inline constexpr auto kProtGpuAll = Protection::GpuRead | Protection::GpuWrite;

#pragma pack(push, 1)
struct QueryResult {
  uint64_t start;
  uint64_t end;
  uint64_t offset;
  uint32_t protection;
  uint32_t memoryType;
  uint32_t flags;
  rx::StaticCString<32> name;
  uint32_t _padding;
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

void initialize(Process *process, bool force = false);
void fork(Process *process, Process *parentThread);

std::pair<rx::AddressRange, ErrorCode>
reserve(Process *process, std::uint64_t addressHint, std::uint64_t size,
        rx::EnumBitSet<AllocationFlags> allocFlags);

std::pair<rx::AddressRange, ErrorCode>
map(Process *process, std::uint64_t addressHint, std::uint64_t size,
    rx::EnumBitSet<AllocationFlags> allocFlags,
    rx::EnumBitSet<Protection> prot = {},
    rx::EnumBitSet<BlockFlags> blockFlags = {},
    std::uint64_t alignment = kPageSize, std::string_view name = {},
    IoDevice *device = nullptr, std::int64_t deviceOffset = 0);
ErrorCode unmap(Process *process, rx::AddressRange range);
ErrorCode setName(Process *process, rx::AddressRange range,
                  std::string_view name);
std::optional<QueryResult> query(Process *process, std::uint64_t address);
std::optional<MemoryProtection> queryProtection(Process *process,
                                                std::uint64_t address);
} // namespace vmem
} // namespace orbis
