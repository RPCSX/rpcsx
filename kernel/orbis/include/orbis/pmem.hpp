#pragma once

#include "error/ErrorCode.hpp"
#include "kernel/MemoryResource.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include <cstdint>

namespace orbis {
using kernel::AllocationFlags;
struct IoDevice;
struct File;
} // namespace orbis

namespace orbis::pmem {
ErrorCode initialize(std::uint64_t size);
void destroy();
std::pair<rx::AddressRange, ErrorCode>
allocate(std::uint64_t addressHint, std::uint64_t size,
         rx::EnumBitSet<AllocationFlags> flags, std::uint64_t alignment);
ErrorCode deallocate(rx::AddressRange range);
std::optional<rx::AddressRange> query(std::uint64_t address);
ErrorCode map(std::uint64_t virtualAddress, rx::AddressRange range,
              rx::EnumBitSet<rx::mem::Protection> protection);
std::size_t getSize();
IoDevice *getDevice();
File *getFile();
} // namespace orbis::pmem
