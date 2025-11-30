#pragma once

#include "dmem.hpp"
#include "error/ErrorCode.hpp"
#include "orbis-config.hpp"
#include "rx/AddressRange.hpp"

namespace orbis {
struct Process;
}

namespace orbis::blockpool {
#pragma pack(push, 1)
struct BlockStats {
  sint availFlushedBlocks;
  sint availCachedBlocks;
  sint commitFlushedBlocks;
  sint commitCachedBlocks;
};
static_assert(sizeof(BlockStats) == 16);
#pragma pack(pop)

void clear();
ErrorCode expand(rx::AddressRange dmemRange);
ErrorCode allocateControlBlock();
ErrorCode releaseControlBlock();
ErrorCode commit(Process *process, rx::AddressRange vmemRange, MemoryType type,
                 rx::EnumBitSet<orbis::vmem::Protection> protection);
void decommit(Process *process, rx::AddressRange vmemRange);
std::optional<MemoryType> getType(std::uint64_t address);
BlockStats stats();
} // namespace orbis::blockpool
