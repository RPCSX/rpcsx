#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>

namespace rx::hash {
class xxhash {
private:
  static constexpr std::uint64_t P1 = 11400714785074694791ULL;
  static constexpr std::uint64_t P2 = 14029467366897019727ULL;
  static constexpr std::uint64_t P3 = 1609587929392839161ULL;
  static constexpr std::uint64_t P4 = 9650029242287828579ULL;
  static constexpr std::uint64_t P5 = 2870177450012600261ULL;

private:
  std::uint64_t mV1 = P1 + P2;
  std::uint64_t mV2 = P2;
  std::uint64_t mV3 = 0;
  std::uint64_t mV4 = 0 - P1;

  alignas(32) std::byte mBuffer[32] = {};
  std::uint64_t mM = 0;
  std::uint64_t mN = 0;

private:
  static constexpr std::uint32_t read32(const void *ptr) {
    std::uint32_t result;
    std::memcpy(&result, ptr, sizeof(result));
    return result;
  }
  static constexpr std::uint64_t read64(const void *ptr) {
    std::uint64_t result;
    std::memcpy(&result, ptr, sizeof(result));
    return result;
  }
  constexpr void init(std::uint64_t seed) {
    mV1 = seed + P1 + P2;
    mV2 = seed + P2;
    mV3 = seed;
    mV4 = seed - P1;
  }

  constexpr static std::uint64_t round(std::uint64_t seed,
                                       std::uint64_t input) {
    seed += input * P2;
    seed = std::rotl(seed, 31);
    seed *= P1;
    return seed;
  }

  constexpr static std::uint64_t mergeRound(std::uint64_t acc,
                                            std::uint64_t val) {
    val = round(0, val);
    acc ^= val;
    acc = acc * P1 + P4;
    return acc;
  }

  constexpr void updateImpl(const std::byte *p, std::size_t k) {
    std::uint64_t v1 = mV1;
    std::uint64_t v2 = mV2;
    std::uint64_t v3 = mV3;
    std::uint64_t v4 = mV4;

    for (std::size_t i = 0; i < k; ++i, p += 32) {
      v1 = round(v1, read64(p + 0));
      v2 = round(v2, read64(p + 8));
      v3 = round(v3, read64(p + 16));
      v4 = round(v4, read64(p + 24));
    }

    mV1 = v1;
    mV2 = v2;
    mV3 = v3;
    mV4 = v4;
  }

public:
  constexpr xxhash() = default;

  explicit constexpr xxhash(std::uint64_t seed) {
    init(seed);
  }

  constexpr void update(const std::byte *p, std::size_t n) {
    if (n == 0)
      return;

    mN += n;

    if (mM > 0) {
      std::size_t k = 32 - mM;

      if (n < k) {
        k = n;
      }

      std::memcpy(mBuffer + mM, p, k);

      p += k;
      n -= k;
      mM += k;

      if (mM < 32)
        return;

      updateImpl(mBuffer, 1);
      mM = 0;
    }

    {
      std::size_t k = n / 32;

      updateImpl(p, k);

      p += 32 * k;
      n -= 32 * k;
    }

    if (n > 0) {
      std::memcpy(mBuffer, p, n);
      mM = n;
    }
  }

  void update(const void *pv, std::size_t n) {
    update(static_cast<const std::byte *>(pv), n);
  }

  void update(std::span<const std::byte> bytes) {
    update(bytes.data(), bytes.size());
  }

  constexpr std::uint64_t end() {
    std::uint64_t h = 0;

    if (mN >= 32) {
      h = std::rotl(mV1, 1) + std::rotl(mV2, 7) + std::rotl(mV3, 12) +
          std::rotl(mV4, 18);

      h = mergeRound(h, mV1);
      h = mergeRound(h, mV2);
      h = mergeRound(h, mV3);
      h = mergeRound(h, mV4);
    } else {
      h = mV3 + P5;
    }

    h += mN;

    auto p = mBuffer;

    std::uint64_t m = mM;

    while (m >= 8) {
      std::uint64_t k1 = round(0, read64(p));

      h ^= k1;
      h = std::rotl(h, 27) * P1 + P4;

      p += 8;
      m -= 8;
    }

    while (m >= 4) {
      h ^= static_cast<std::uint64_t>(read32(p)) * P1;
      h = std::rotl(h, 23) * P2 + P3;

      p += 4;
      m -= 4;
    }

    while (m > 0) {
      h ^= static_cast<unsigned char>(p[0]) * P5;
      h = std::rotl(h, 11) * P1;

      ++p;
      --m;
    }

    mN += 32 - mM;
    mM = 0;

    std::memset(mBuffer, 0, 32);

    mV1 += h;
    mV2 += h;
    mV3 -= h;
    mV4 -= h;

    h ^= h >> 33;
    h *= P2;
    h ^= h >> 29;
    h *= P3;
    h ^= h >> 32;
    return h;
  }
};
} // namespace rx::hash
