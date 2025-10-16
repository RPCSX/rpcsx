#pragma once

#include "AddressRange.hpp"
#include "EnumBitSet.hpp"
#include <cstddef>
#include <system_error>
#include <vector>

namespace rx::mem {
enum class Protection {
  R,
  W,
  X,

  bitset_last = X
};

struct VirtualQueryEntry : rx::AddressRange {
  rx::EnumBitSet<Protection> flags{};

  VirtualQueryEntry() = default;
  VirtualQueryEntry(rx::AddressRange range, rx::EnumBitSet<Protection> prot)
      : AddressRange(range), flags(prot) {}
};

extern const std::size_t pageSize;
std::errc reserve(rx::AddressRange range);
std::errc release(rx::AddressRange range, std::size_t alignment);
std::errc protect(rx::AddressRange range, rx::EnumBitSet<Protection> prot);
std::vector<VirtualQueryEntry> query(rx::AddressRange range);
} // namespace rx::mem
