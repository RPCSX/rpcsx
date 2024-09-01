#pragma once

#include <cstddef>
#include <span>

namespace rx {
void hexdump(std::span<std::byte> bytes);
} // namespace rx
