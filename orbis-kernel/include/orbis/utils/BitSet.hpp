#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>

namespace orbis {
inline namespace utils {
template <std::size_t Count> struct BitSet {
  using chunk_type = std::uint64_t;
  static constexpr auto BitsPerChunk = sizeof(chunk_type) * 8;
  static constexpr auto ChunkCount = (Count + BitsPerChunk - 1) / BitsPerChunk;
  chunk_type _bits[ChunkCount]{};

  constexpr std::size_t countr_one() const {
    std::size_t result = 0;
    for (auto bits : _bits) {
      auto count = std::countr_one(bits);
      result += count;

      if (count != BitsPerChunk) {
        break;
      }
    }

    return result;
  }

  constexpr std::size_t countr_zero(std::size_t offset = 0) const {
    std::size_t result = 0;

    if (auto chunkOffset = offset % BitsPerChunk) {
      auto count =
          std::countr_zero(_bits[offset / BitsPerChunk] >> chunkOffset);

      if (count != BitsPerChunk) {
        return count + offset;
      }

      offset = (offset + BitsPerChunk - 1) & ~(BitsPerChunk - 1);
    }

    for (auto i = offset / BitsPerChunk; i < ChunkCount; ++i) {
      auto count = std::countr_zero(_bits[i]);
      result += count;

      if (count != BitsPerChunk) {
        break;
      }
    }
    /*
    for (auto bits : _bits) {
      auto count = std::countr_zero(bits);
      result += count;

      if (count != BitsPerChunk) {
        break;
      }
    }
    */

    return result + offset;
  }

  bool empty() const {
    for (auto bits : _bits) {
      if (bits != 0) {
        return false;
      }
    }

    return true;
  }

  bool full() const {
    if constexpr (Count < BitsPerChunk) {
      return _bits[0] == (static_cast<std::uint64_t>(1) << Count) - 1;
    }

    for (auto bits : _bits) {
      if (bits != ~static_cast<chunk_type>(0)) {
        return false;
      }
    }

    return true;
  }

  constexpr void clear(std::size_t index) {
    _bits[index / BitsPerChunk] &=
        ~(static_cast<chunk_type>(1) << (index % BitsPerChunk));
  }

  constexpr void set(std::size_t index) {
    _bits[index / BitsPerChunk] |= static_cast<chunk_type>(1)
                                   << (index % BitsPerChunk);
  }

  constexpr bool test(std::size_t index) const {
    return (_bits[index / BitsPerChunk] &
            (static_cast<chunk_type>(1) << (index % BitsPerChunk))) != 0;
  }
};
} // namespace utils
} // namespace orbis
