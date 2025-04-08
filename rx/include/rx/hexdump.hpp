#pragma once

#include <cstddef>
#include <span>

namespace rx {
void hexdump(std::span<const std::byte> bytes);
inline void hexdump(const void *data, std::size_t size) {
  hexdump({reinterpret_cast<const std::byte *>(data), size});
}
} // namespace rx
