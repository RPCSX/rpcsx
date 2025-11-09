#pragma once

#include "error/ErrorCode.hpp"
#include "kernel/MemoryResource.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include <cstdint>

namespace orbis {
using kernel::AllocationFlags;
struct IoDevice;
} // namespace orbis

namespace orbis::pmem {
enum class MemoryType : std::uint32_t {
  Invalid = -1u,
  WbOnion = 0,   // write back, CPU bus
  WCGarlic = 3,  // combining, GPU bus
  WbGarlic = 10, // write back, GPU bus
};

struct AllocatedMemory {
  rx::AddressRange range;
  MemoryType memoryType;
};

ErrorCode initialize(std::uint64_t size);
void destroy();
std::pair<rx::AddressRange, ErrorCode>
allocate(std::uint64_t addressHint, std::uint64_t size, MemoryType memoryType,
         rx::EnumBitSet<AllocationFlags> flags, std::uint64_t alignment);
ErrorCode deallocate(rx::AddressRange range);
std::optional<AllocatedMemory> query(std::uint64_t address);
ErrorCode map(std::uint64_t virtualAddress, rx::AddressRange range,
              rx::EnumBitSet<rx::mem::Protection> protection);
std::size_t getSize();
IoDevice *getDevice();
} // namespace orbis::pmem
