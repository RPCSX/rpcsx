#pragma once

#include <cstddef>
#include <type_traits>

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif

namespace rx {
template <typename T, std::size_t N> struct BitFieldBase {
  using type = T;
  using vtype = std::common_type_t<type>;
  using utype = std::make_unsigned_t<vtype>;

  static constexpr bool can_be_packed =
      N < (sizeof(int) * 8 + (std::is_unsigned_v<vtype> ? 1 : 0)) &&
      sizeof(vtype) > sizeof(int);
  using compact_type = std::conditional_t<
      can_be_packed,
      std::conditional_t<std::is_unsigned_v<vtype>, std::size_t, int>, vtype>;

  // Datatype bitsize
  static constexpr std::size_t bitmax = sizeof(T) * 8;
  static_assert(N - 1 < bitmax, "BitFieldBase<> error: N out of bounds");

  // Field bitsize
  static constexpr std::size_t bitsize = N;

  // All ones mask
  static constexpr utype mask1 = static_cast<utype>(~static_cast<utype>(0));

  // Value mask
  static constexpr utype vmask = mask1 >> (bitmax - bitsize);

protected:
  type m_data;
};

// Bitfield accessor (N bits from I position, 0 is LSB)
template <typename T, std::size_t I, std::size_t N>
struct BitField : BitFieldBase<T, N> {
  using type = typename BitField::type;
  using vtype = typename BitField::vtype;
  using utype = typename BitField::utype;
  using compact_type = typename BitField::compact_type;

  // Field offset
  static constexpr std::size_t bitpos = I;
  static_assert(bitpos + N <= BitField::bitmax,
                "BitField<> error: I out of bounds");

  // Get bitmask of size N, at I pos
  static constexpr utype data_mask() {
    return static_cast<utype>(
        static_cast<utype>(BitField::mask1 >>
                           (BitField::bitmax - BitField::bitsize))
        << bitpos);
  }

  // Bitfield extraction
  static constexpr compact_type extract(const T &data) noexcept {
    if constexpr (std::is_signed_v<T>) {
      // Load signed value (sign-extended)
      return static_cast<compact_type>(
          static_cast<vtype>(static_cast<utype>(data)
                             << (BitField::bitmax - bitpos - N)) >>
          (BitField::bitmax - N));
    } else {
      // Load unsigned value
      return static_cast<compact_type>((static_cast<utype>(data) >> bitpos) &
                                       BitField::vmask);
    }
  }

  // Bitfield insertion
  static constexpr vtype insert(compact_type value) {
    return static_cast<vtype>((value & BitField::vmask) << bitpos);
  }

  // Load bitfield value
  constexpr operator compact_type() const noexcept {
    return extract(this->m_data);
  }

  // Load raw data with mask applied
  constexpr T unshifted() const {
    return static_cast<T>(this->m_data & data_mask());
  }

  // Optimized bool conversion (must be removed if inappropriate)
  explicit constexpr operator bool() const noexcept {
    return unshifted() != 0u;
  }

  // Store bitfield value
  BitField &operator=(compact_type value) noexcept {
    this->m_data =
        static_cast<vtype>((this->m_data & ~data_mask()) | insert(value));
    return *this;
  }

  compact_type operator++(int) {
    compact_type result = *this;
    *this = static_cast<compact_type>(result + 1u);
    return result;
  }

  BitField &operator++() {
    return *this = static_cast<compact_type>(*this + 1u);
  }

  compact_type operator--(int) {
    compact_type result = *this;
    *this = static_cast<compact_type>(result - 1u);
    return result;
  }

  BitField &operator--() {
    return *this = static_cast<compact_type>(*this - 1u);
  }

  BitField &operator+=(compact_type right) {
    return *this = static_cast<compact_type>(*this + right);
  }

  BitField &operator-=(compact_type right) {
    return *this = static_cast<compact_type>(*this - right);
  }

  BitField &operator*=(compact_type right) {
    return *this = static_cast<compact_type>(*this * right);
  }

  BitField &operator&=(compact_type right) {
    this->m_data &= static_cast<vtype>(
        ((static_cast<utype>(right + 0u) & BitField::vmask) << bitpos) |
        ~(BitField::vmask << bitpos));
    return *this;
  }

  BitField &operator|=(compact_type right) {
    this->m_data |= static_cast<vtype>(
        (static_cast<utype>(right + 0u) & BitField::vmask) << bitpos);
    return *this;
  }

  BitField &operator^=(compact_type right) {
    this->m_data ^= static_cast<vtype>(
        (static_cast<utype>(right + 0u) & BitField::vmask) << bitpos);
    return *this;
  }
};

// Field pack (concatenated from left to right)
template <typename F = void, typename... Fields>
struct BitFieldPack
    : BitFieldBase<typename F::type,
                   F::bitsize + BitFieldPack<Fields...>::bitsize> {
  using type = typename BitFieldPack::type;
  using vtype = typename BitFieldPack::vtype;
  using utype = typename BitFieldPack::utype;
  using compact_type = typename BitFieldPack::compact_type;

  // Get disjunction of all "data" masks of concatenated values
  static constexpr vtype data_mask() {
    return static_cast<vtype>(F::data_mask() |
                              BitFieldPack<Fields...>::data_mask());
  }

  // Extract all bitfields and concatenate
  static constexpr compact_type extract(const type &data) {
    return static_cast<compact_type>(static_cast<utype>(F::extract(data))
                                         << BitFieldPack<Fields...>::bitsize |
                                     BitFieldPack<Fields...>::extract(data));
  }

  // Split bitfields and insert them
  static constexpr vtype insert(compact_type value) {
    return static_cast<vtype>(
        F::insert(value >> BitFieldPack<Fields...>::bitsize) |
        BitFieldPack<Fields...>::insert(value));
  }

  // Load value
  constexpr operator compact_type() const noexcept {
    return extract(this->m_data);
  }

  // Store value
  BitFieldPack &operator=(compact_type value) noexcept {
    this->m_data = (this->m_data & ~data_mask()) | insert(value);
    return *this;
  }
};

// Empty field pack (recursion terminator)
template <> struct BitFieldPack<void> {
  static constexpr std::size_t bitsize = 0;

  static constexpr std::size_t data_mask() { return 0; }

  template <typename T>
  static constexpr auto extract(const T &) -> decltype(+T()) {
    return 0;
  }

  template <typename T> static constexpr T insert(T /*value*/) { return 0; }
};

// Fixed field (provides constant values in field pack)
template <typename T, T V, std::size_t N>
struct BitFieldFixed : BitFieldBase<T, N> {
  using type = typename BitFieldFixed::type;
  using vtype = typename BitFieldFixed::vtype;

  // Return constant value
  static constexpr vtype extract(const type &) {
    static_assert((V & BitFieldFixed::vmask) == V,
                  "BitFieldFixed<> error: V out of bounds");
    return V;
  }

  // Get value
  constexpr operator vtype() const noexcept { return V; }
};
} // namespace rx

template <typename T, std::size_t I, std::size_t N>
struct std::common_type<rx::BitField<T, I, N>, rx::BitField<T, I, N>>
    : std::common_type<T> {};

template <typename T, std::size_t I, std::size_t N, typename T2>
struct std::common_type<rx::BitField<T, I, N>, T2>
    : std::common_type<T2, std::common_type_t<T>> {};

template <typename T, std::size_t I, std::size_t N, typename T2>
struct std::common_type<T2, rx::BitField<T, I, N>>
    : std::common_type<std::common_type_t<T>, T2> {};

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
