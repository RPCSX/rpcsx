#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace rx {
namespace detail {
template <std::size_t Count> auto pickBitSetBaseType() {
  if constexpr (Count <= 8) {
    return std::array<std::atomic<std::uint8_t>, 1>{};
  } else if constexpr (Count <= 16) {
    return std::array<std::atomic<std::uint16_t>, 1>{};
  } else if constexpr (Count <= 32) {
    return std::array<std::atomic<std::uint32_t>, 1>{};
  } else {
    return std::array<std::atomic<std::uint64_t>, (Count + 63) / 64>();
  }
}

template <std::size_t Count>
using ConcurrentBitPoolBaseType = decltype(pickBitSetBaseType<Count>());
} // namespace detail

template <std::size_t BitCount, typename ElementType = std::size_t>
class ConcurrentBitPool {
  detail::ConcurrentBitPoolBaseType<BitCount> mStorage{{}};
  using WordType = std::remove_cvref_t<decltype(mStorage[0])>::value_type;
  static constexpr auto kWordBitWidth = sizeof(WordType) * 8;

public:
  ElementType acquire() {
    while (true) {
      for (auto &node : mStorage) {
        auto mask = node.load(std::memory_order::acquire);

        auto bitIndex = std::countr_one(mask);
        if (bitIndex >= kWordBitWidth) {
          continue;
        }

        auto pattern = static_cast<WordType>(1) << bitIndex;

        if (!node.compare_exchange_strong(mask, mask | pattern,
                                          std::memory_order::release,
                                          std::memory_order::relaxed)) {
          continue;
        }

        auto wordIndex = &node - mStorage.data();
        return static_cast<ElementType>(kWordBitWidth * wordIndex + bitIndex);
      }
    }
  }

  void release(ElementType index) {
    auto rawIndex = static_cast<std::size_t>(index);
    auto bitIndex = rawIndex % kWordBitWidth;
    auto wordIndex = rawIndex / kWordBitWidth;

    WordType pattern = static_cast<WordType>(1) << bitIndex;
    WordType mask = pattern;

    while (!mStorage[wordIndex].compare_exchange_weak(
        mask, mask & ~pattern, std::memory_order::release,
        std::memory_order::acquire)) {
    }
  }
};
} // namespace rx
