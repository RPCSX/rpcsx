#pragma once

#include "error/ErrorCode.hpp"
#include "rx/AddressRange.hpp"
#include <cstdint>

namespace orbis::fmem {
ErrorCode initialize(std::uint64_t size);
void destroy();
std::pair<rx::AddressRange, ErrorCode> allocate(std::uint64_t size);
ErrorCode deallocate(rx::AddressRange range);
} // namespace orbis::fmem
