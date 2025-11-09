#pragma once

#include "error/ErrorCode.hpp"
#include "rx/AddressRange.hpp"
#include <cstdint>

namespace orbis {
struct Process;
}
namespace orbis::fmem {
ErrorCode initialize(Process *process, std::uint64_t size);
void destroy(Process *process);
std::pair<rx::AddressRange, ErrorCode> allocate(Process *process,
                                                std::uint64_t size);
ErrorCode deallocate(Process *process, rx::AddressRange range);
} // namespace orbis::fmem
