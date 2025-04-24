#pragma once

/*
This header implements bs_t<> class for scoped enum types (enum class).
To enable bs_t<>, enum scope must contain `__bitset_enum_max` entry.

enum class flagzz : u32
{
    flag1, // Bit indices start from zero
    flag2,
};

This also enables helper operators for this enum type.

Examples:
`flagzz::flag1 | flagzz::flag2` - bitset union
`flagzz::flag1 & ~flagzz::flag2` - bitset difference
Intersection (&) and symmetric difference (^) is also available.
*/

#include "refl.hpp"
#include "types.hpp"

namespace rx {
template <typename T>
concept BitSetEnum =
    std::is_enum_v<T> && requires(T x) { rx::fieldCount<T> > 0; };

template <BitSetEnum T> class BitSet;

namespace detail {
template <BitSetEnum T> class InvertedBitSet final {
  using underlying_type = std::underlying_type_t<T>;
  underlying_type m_data;
  constexpr InvertedBitSet(underlying_type data) : m_data(data) {}
  friend BitSet<T>;
};
} // namespace detail

// Bitset type for enum class with available bits [0, fieldCount)
template <BitSetEnum T> class BitSet final {
public:
  // Underlying type
  using underlying_type = std::underlying_type_t<T>;

private:
  // Underlying value
  underlying_type m_data;

  // Value constructor
  constexpr explicit BitSet(int, underlying_type data) noexcept
      : m_data(data) {}

public:
  static constexpr usz bitmax = sizeof(T) * 8;
  static constexpr usz bitsize =
      static_cast<underlying_type>(rx::fieldCount<T>);

  static_assert(std::is_enum_v<T>,
                "BitSet<> error: invalid type (must be enum)");
  static_assert(bitsize <= bitmax,
                "BitSet<> error: failed to determine enum field count");
  static_assert(bitsize != bitmax || std::is_unsigned_v<underlying_type>,
                "BitSet<> error: invalid field count (sign bit)");

  // Helper function
  static constexpr underlying_type shift(T value) {
    return static_cast<underlying_type>(1)
           << static_cast<underlying_type>(value);
  }

  BitSet() = default;

  // Construct from a single bit
  constexpr BitSet(T bit) noexcept : m_data(shift(bit)) {}

  // Test for empty bitset
  constexpr explicit operator bool() const noexcept { return m_data != 0; }

  // Extract underlying data
  constexpr explicit operator underlying_type() const noexcept {
    return m_data;
  }

  constexpr detail::InvertedBitSet<T> operator~() const { return {m_data}; }

  constexpr BitSet &operator+=(BitSet rhs) {
    m_data |= static_cast<underlying_type>(rhs);
    return *this;
  }

  constexpr BitSet &operator-=(BitSet rhs) {
    m_data &= ~static_cast<underlying_type>(rhs);
    return *this;
  }

  constexpr BitSet without(BitSet rhs) const {
    BitSet result = *this;
    result.m_data &= ~static_cast<underlying_type>(rhs);
    return result;
  }

  constexpr BitSet with(BitSet rhs) const {
    BitSet result = *this;
    result.m_data |= static_cast<underlying_type>(rhs);
    return result;
  }

  constexpr BitSet &operator&=(BitSet rhs) {
    m_data &= static_cast<underlying_type>(rhs);
    return *this;
  }

  constexpr BitSet &operator^=(BitSet rhs) {
    m_data ^= static_cast<underlying_type>(rhs);
    return *this;
  }

  [[deprecated("Use operator|")]] friend constexpr BitSet
  operator+(BitSet lhs, BitSet rhs) {
    return BitSet(0, lhs.m_data | rhs.m_data);
  }

  friend constexpr BitSet operator-(BitSet lhs, BitSet rhs) {
    return BitSet(0, lhs.m_data & ~rhs.m_data);
  }

  friend constexpr BitSet operator|(BitSet lhs, BitSet rhs) {
    return BitSet(0, lhs.m_data | rhs.m_data);
  }

  friend constexpr BitSet operator&(BitSet lhs, BitSet rhs) {
    return BitSet(0, lhs.m_data & rhs.m_data);
  }

  friend constexpr BitSet operator&(BitSet lhs, detail::InvertedBitSet<T> rhs) {
    return BitSet(0, lhs.m_data & rhs.m_data);
  }

  friend constexpr BitSet operator^(BitSet lhs, BitSet rhs) {
    return BitSet(0, lhs.m_data ^ rhs.m_data);
  }

  constexpr bool operator==(BitSet rhs) const noexcept {
    return m_data == rhs.m_data;
  }

  constexpr bool test_and_set(T bit) {
    bool r = (m_data & shift(bit)) != 0;
    m_data |= shift(bit);
    return r;
  }

  constexpr bool test_and_reset(T bit) {
    bool r = (m_data & shift(bit)) != 0;
    m_data &= ~shift(bit);
    return r;
  }

  constexpr bool test_and_complement(T bit) {
    bool r = (m_data & shift(bit)) != 0;
    m_data ^= shift(bit);
    return r;
  }

  constexpr bool any_of(BitSet arg) const { return (m_data & arg.m_data) != 0; }

  constexpr bool all_of(BitSet arg) const {
    return (m_data & arg.m_data) == arg.m_data;
  }

  constexpr bool none_of(BitSet arg) const {
    return (m_data & arg.m_data) == 0;
  }
};

namespace bitset {
// Unary '+' operator: promote plain enum value to bitset value
template <BitSetEnum T>
[[deprecated("Use toBitSet(bit)")]] constexpr BitSet<T> operator+(T bit) {
  return BitSet<T>(bit);
}

template <BitSetEnum T> constexpr BitSet<T> toBitSet(T bit) {
  return BitSet<T>(bit);
}

// Binary '+' operator: bitset union
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<BitSet<T>, U>)
[[deprecated("Use operator|")]] constexpr BitSet<T> operator+(T lhs,
                                                              const U &rhs) {
  return BitSet<T>(lhs) | BitSet<T>(rhs);
}

// Binary '+' operator: bitset union
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<BitSet<T>, U> && !std::is_enum_v<U>)
[[deprecated("Use operator|")]] constexpr BitSet<T> operator+(const U &lhs,
                                                              T rhs) {
  return BitSet<T>(lhs) | BitSet<T>(rhs);
}

// Binary '|' operator: bitset union
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<BitSet<T>, U>)
constexpr BitSet<T> operator|(T lhs, const U &rhs) {
  return BitSet<T>(lhs) | BitSet<T>(rhs);
}

// Binary '|' operator: bitset union
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<BitSet<T>, U> && !std::is_enum_v<U>)
constexpr BitSet<T> operator|(const U &lhs, T rhs) {
  return BitSet<T>(lhs) | BitSet<T>(rhs);
}

// Binary '-' operator: bitset difference
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<BitSet<T>, U>)
constexpr BitSet<T> operator-(T lhs, const U &rhs) {
  return BitSet<T>(lhs) - BitSet<T>(rhs);
}

// Binary '-' operator: bitset difference
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<BitSet<T>, U> && !std::is_enum_v<U>)
constexpr BitSet<T> operator-(const U &lhs, T rhs) {
  return BitSet<T>(lhs) - BitSet<T>(rhs);
}

// Binary '&' operator: bitset intersection
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<BitSet<T>, U>)
constexpr BitSet<T> operator&(T lhs, const U &rhs) {
  return BitSet<T>(lhs) & BitSet<T>(rhs);
}

// Binary '&' operator: bitset intersection
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<BitSet<T>, U> && !std::is_enum_v<U>)
constexpr BitSet<T> operator&(const U &lhs, T rhs) {
  return BitSet<T>(lhs) & BitSet<T>(rhs);
}

// Binary '&' operator: bitset intersection
template <BitSetEnum T, typename U>
constexpr BitSet<T> operator&(T lhs, detail::InvertedBitSet<T> rhs) {
  return BitSet<T>(lhs) & rhs;
}

// Binary '^' operator: bitset symmetric difference
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<BitSet<T>, U>)
constexpr BitSet<T> operator^(T lhs, const U &rhs) {
  return BitSet<T>(lhs) ^ BitSet<T>(rhs);
}

// Binary '^' operator: bitset symmetric difference
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<BitSet<T>, U> && !std::is_enum_v<U>)
constexpr BitSet<T> operator^(const U &lhs, T rhs) {
  return BitSet<T>(lhs) ^ BitSet<T>(rhs);
}
} // namespace bitset
} // namespace rx

using namespace rx::bitset;
