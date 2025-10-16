#pragma once

/*
This header implements EnumBitSet<> class for scoped enum types (enum class).
To enable EnumBitSet<>, enum scope must contain `bitset_last` entry.

enum class flagzz : u32
{
    flag1, // Bit indices start from zero
    flag2,

    bitset_last = flag2
};

This also enables helper operators for this enum type.

Examples:
`flagzz::flag1 | flagzz::flag2` - bitset union
`flagzz::flag1 & ~flagzz::flag2` - bitset difference
Intersection (&) and symmetric difference (^) is also available.
*/

#include "types.hpp"

namespace rx {
template <typename T>
concept BitSetEnum = std::is_enum_v<T> && requires { T::bitset_last; };

template <BitSetEnum T> class EnumBitSet;

namespace detail {
template <BitSetEnum T> class InvertedEnumBitSet final {
  using underlying_type = std::underlying_type_t<T>;
  underlying_type m_data;
  constexpr InvertedEnumBitSet(underlying_type data) : m_data(data) {}
  friend EnumBitSet<T>;
};
} // namespace detail

// Bitset type for enum class with available bits [0, fieldCount)
template <BitSetEnum T> class EnumBitSet final {
public:
  // Underlying type
  using underlying_type = std::underlying_type_t<T>;

private:
  // Underlying value
  underlying_type m_data;

  // Value constructor
  constexpr explicit EnumBitSet(int, underlying_type data) noexcept
      : m_data(data) {}

public:
  static constexpr usz bitmax = sizeof(T) * 8;
  static constexpr usz bitsize =
      static_cast<underlying_type>(T::bitset_last) + 1;

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

  EnumBitSet() = default;

  // Construct from a single bit
  constexpr EnumBitSet(T bit) noexcept : m_data(shift(bit)) {}

  [[nodiscard]] constexpr underlying_type toUnderlying() const {
    return m_data;
  }

  [[nodiscard]] static constexpr EnumBitSet
  fromUnderlying(underlying_type raw) {
    return EnumBitSet(0, raw);
  }

  // Test for empty bitset
  constexpr explicit operator bool() const noexcept { return m_data != 0; }

  // Extract underlying data
  constexpr explicit operator underlying_type() const noexcept {
    return m_data;
  }

  constexpr detail::InvertedEnumBitSet<T> operator~() const { return {m_data}; }

  [[deprecated("Use operator|=")]] constexpr EnumBitSet &
  operator+=(EnumBitSet rhs) {
    m_data |= static_cast<underlying_type>(rhs);
    return *this;
  }

  constexpr EnumBitSet &operator|=(EnumBitSet rhs) {
    m_data |= static_cast<underlying_type>(rhs);
    return *this;
  }

  constexpr EnumBitSet &operator-=(EnumBitSet rhs) {
    m_data &= ~static_cast<underlying_type>(rhs);
    return *this;
  }

  constexpr EnumBitSet without(EnumBitSet rhs) const {
    EnumBitSet result = *this;
    result.m_data &= ~static_cast<underlying_type>(rhs);
    return result;
  }

  constexpr EnumBitSet with(EnumBitSet rhs) const {
    EnumBitSet result = *this;
    result.m_data |= static_cast<underlying_type>(rhs);
    return result;
  }

  constexpr EnumBitSet &operator&=(EnumBitSet rhs) {
    m_data &= static_cast<underlying_type>(rhs);
    return *this;
  }

  constexpr EnumBitSet &operator^=(EnumBitSet rhs) {
    m_data ^= static_cast<underlying_type>(rhs);
    return *this;
  }

  [[deprecated("Use operator|")]] friend constexpr EnumBitSet
  operator+(EnumBitSet lhs, EnumBitSet rhs) {
    return EnumBitSet(0, lhs.m_data | rhs.m_data);
  }

  friend constexpr EnumBitSet operator-(EnumBitSet lhs, EnumBitSet rhs) {
    return EnumBitSet(0, lhs.m_data & ~rhs.m_data);
  }

  friend constexpr EnumBitSet operator|(EnumBitSet lhs, EnumBitSet rhs) {
    return EnumBitSet(0, lhs.m_data | rhs.m_data);
  }

  friend constexpr EnumBitSet operator&(EnumBitSet lhs, EnumBitSet rhs) {
    return EnumBitSet(0, lhs.m_data & rhs.m_data);
  }

  friend constexpr EnumBitSet operator&(EnumBitSet lhs,
                                        detail::InvertedEnumBitSet<T> rhs) {
    return EnumBitSet(0, lhs.m_data & rhs.m_data);
  }

  friend constexpr EnumBitSet operator^(EnumBitSet lhs, EnumBitSet rhs) {
    return EnumBitSet(0, lhs.m_data ^ rhs.m_data);
  }

  constexpr bool operator==(EnumBitSet rhs) const noexcept {
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

  constexpr bool any_of(EnumBitSet arg) const {
    return (m_data & arg.m_data) != 0;
  }

  constexpr bool all_of(EnumBitSet arg) const {
    return (m_data & arg.m_data) == arg.m_data;
  }

  constexpr bool none_of(EnumBitSet arg) const {
    return (m_data & arg.m_data) == 0;
  }

  underlying_type &raw() { return m_data; }
};

template <BitSetEnum T> constexpr EnumBitSet<T> toBitSet(T bit) {
  return EnumBitSet<T>(bit);
}

namespace bitset {
// Unary '+' operator: promote plain enum value to bitset value
template <BitSetEnum T>
[[deprecated("Use toBitSet(bit)")]] constexpr EnumBitSet<T> operator+(T bit) {
  return EnumBitSet<T>(bit);
}
template <BitSetEnum T>
constexpr detail::InvertedEnumBitSet<T> operator~(T bit) {
  return ~toBitSet(bit);
}
// Binary '+' operator: bitset union
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<EnumBitSet<T>, U>)
[[deprecated("Use operator|")]] constexpr EnumBitSet<T>
operator+(T lhs, const U &rhs) {
  return EnumBitSet<T>(lhs) | EnumBitSet<T>(rhs);
}

// Binary '+' operator: bitset union
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<EnumBitSet<T>, U> && !std::is_enum_v<U>)
[[deprecated("Use operator|")]] constexpr EnumBitSet<T> operator+(const U &lhs,
                                                                  T rhs) {
  return EnumBitSet<T>(lhs) | EnumBitSet<T>(rhs);
}

// Binary '|' operator: bitset union
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<EnumBitSet<T>, U>)
constexpr EnumBitSet<T> operator|(T lhs, const U &rhs) {
  return EnumBitSet<T>(lhs) | EnumBitSet<T>(rhs);
}

// Binary '|' operator: bitset union
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<EnumBitSet<T>, U> && !std::is_enum_v<U>)
constexpr EnumBitSet<T> operator|(const U &lhs, T rhs) {
  return EnumBitSet<T>(lhs) | EnumBitSet<T>(rhs);
}

// Binary '-' operator: bitset difference
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<EnumBitSet<T>, U>)
constexpr EnumBitSet<T> operator-(T lhs, const U &rhs) {
  return EnumBitSet<T>(lhs) - EnumBitSet<T>(rhs);
}

// Binary '-' operator: bitset difference
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<EnumBitSet<T>, U> && !std::is_enum_v<U>)
constexpr EnumBitSet<T> operator-(const U &lhs, T rhs) {
  return EnumBitSet<T>(lhs) - EnumBitSet<T>(rhs);
}

// Binary '&' operator: bitset intersection
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<EnumBitSet<T>, U>)
constexpr EnumBitSet<T> operator&(T lhs, const U &rhs) {
  return EnumBitSet<T>(lhs) & EnumBitSet<T>(rhs);
}

// Binary '&' operator: bitset intersection
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<EnumBitSet<T>, U> && !std::is_enum_v<U>)
constexpr EnumBitSet<T> operator&(const U &lhs, T rhs) {
  return EnumBitSet<T>(lhs) & EnumBitSet<T>(rhs);
}

// Binary '&' operator: bitset intersection
template <BitSetEnum T, typename U>
constexpr EnumBitSet<T> operator&(T lhs, detail::InvertedEnumBitSet<T> rhs) {
  return EnumBitSet<T>(lhs) & rhs;
}

// Binary '^' operator: bitset symmetric difference
template <BitSetEnum T, typename U>
  requires(std::is_constructible_v<EnumBitSet<T>, U>)
constexpr EnumBitSet<T> operator^(T lhs, const U &rhs) {
  return EnumBitSet<T>(lhs) ^ EnumBitSet<T>(rhs);
}

// Binary '^' operator: bitset symmetric difference
template <typename U, BitSetEnum T>
  requires(std::is_constructible_v<EnumBitSet<T>, U> && !std::is_enum_v<U>)
constexpr EnumBitSet<T> operator^(const U &lhs, T rhs) {
  return EnumBitSet<T>(lhs) ^ EnumBitSet<T>(rhs);
}
} // namespace bitset
} // namespace rx

using namespace rx::bitset;
