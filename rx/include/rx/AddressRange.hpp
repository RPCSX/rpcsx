#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace rx {
class AddressRange {
  std::uint64_t mBeginAddress = -1;
  std::uint64_t mEndAddress = 0;

public:
  constexpr AddressRange() = default;

  [[nodiscard]] static constexpr AddressRange
  fromBeginSize(std::uint64_t begin, std::uint64_t size) {
    AddressRange result;
    result.mBeginAddress = begin;
    result.mEndAddress = begin + size;
    return result;
  }
  [[nodiscard]] static constexpr AddressRange fromBeginEnd(std::uint64_t begin,
                                                           std::uint64_t end) {
    AddressRange result;
    result.mBeginAddress = begin;
    result.mEndAddress = end;
    return result;
  }

  [[nodiscard]] constexpr bool isValid() const {
    return mBeginAddress < mEndAddress;
  }
  constexpr explicit operator bool() const { return isValid(); }

  [[nodiscard]] constexpr bool intersects(AddressRange other) const {
    return mBeginAddress < other.mEndAddress &&
           mEndAddress > other.mBeginAddress;
  }
  [[nodiscard]] constexpr bool contains(AddressRange other) const {
    return mBeginAddress <= other.mBeginAddress &&
           mEndAddress >= other.mEndAddress;
  }
  [[nodiscard]] constexpr bool contains(std::uint64_t address) const {
    return address >= mBeginAddress && address < mEndAddress;
  }

  [[nodiscard]] constexpr AddressRange merge(AddressRange other) const {
    return fromBeginEnd(std::min(mBeginAddress, other.mBeginAddress),
                        std::max(mEndAddress, other.mEndAddress));
  }

  [[nodiscard]] constexpr AddressRange intersection(AddressRange other) const {
    return fromBeginEnd(std::max(mBeginAddress, other.mBeginAddress),
                        std::min(mEndAddress, other.mEndAddress));
  }

  [[nodiscard]] constexpr std::size_t size() const {
    return mEndAddress - mBeginAddress;
  }
  [[nodiscard]] constexpr std::size_t beginAddress() const {
    return mBeginAddress;
  }
  [[nodiscard]] constexpr std::size_t endAddress() const { return mEndAddress; }

  constexpr bool operator==(const AddressRange &) const = default;
};
} // namespace rx
