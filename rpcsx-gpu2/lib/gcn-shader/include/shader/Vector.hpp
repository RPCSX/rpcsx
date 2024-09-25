#pragma once

#include <array>
#include <cstdint>

namespace shader {
template <typename T, std::size_t N> struct Vector : std::array<T, N> {
  using std::array<T, N>::array;

  template<typename U>
  constexpr explicit operator Vector<U, N>() const {
     Vector<U, N> result;
     for (std::size_t i = 0; i < N; ++i) {
       result[i] = static_cast<U>((*this)[i]);
     }
     return result;
  }

#define DEFINE_BINOP(OP)                                                       \
  constexpr auto operator OP(const Vector &other) const                        \
    requires requires(T lhs, T rhs) { lhs OP rhs; }                            \
  {                                                                            \
    using ResultElementT =                                                     \
        std::remove_cvref_t<decltype(std::declval<T>() OP std::declval<T>())>; \
    Vector<ResultElementT, N> result;                                          \
    for (std::size_t i = 0; i < N; ++i) {                                      \
      result[i] = (*this)[i] OP other[i];                                      \
    }                                                                          \
    return result;                                                             \
  }                                                                            \
  constexpr auto operator OP(const T &other) const                             \
    requires requires(T lhs, T rhs) { lhs OP rhs; }                            \
  {                                                                            \
    using ResultElementT =                                                     \
        std::remove_cvref_t<decltype(std::declval<T>() OP std::declval<T>())>; \
    Vector<ResultElementT, N> result;                                          \
    for (std::size_t i = 0; i < N; ++i) {                                      \
      result[i] = (*this)[i] OP other;                                         \
    }                                                                          \
    return result;                                                             \
  }

#define DEFINE_UNOP(OP)                                                        \
  constexpr auto operator OP() const                                           \
    requires requires(T rhs) { OP rhs; }                                       \
  {                                                                            \
    using ResultElementT =                                                     \
        std::remove_cvref_t<decltype(OP std::declval<T>())>;                   \
    Vector<ResultElementT, N> result;                                          \
    for (std::size_t i = 0; i < N; ++i) {                                      \
      result[i] = OP(*this)[i];                                                \
    }                                                                          \
    return result;                                                             \
  }

  DEFINE_BINOP(+)
  DEFINE_BINOP(-)
  DEFINE_BINOP(*)
  DEFINE_BINOP(/)
  DEFINE_BINOP(%)
  DEFINE_BINOP(&)
  DEFINE_BINOP(|)
  DEFINE_BINOP(^)
  DEFINE_BINOP(>>)
  DEFINE_BINOP(<<)
  DEFINE_BINOP(&&)
  DEFINE_BINOP(||)
  DEFINE_BINOP(<)
  DEFINE_BINOP(>)
  DEFINE_BINOP(<=)
  DEFINE_BINOP(>=)
  DEFINE_BINOP(==)
  DEFINE_BINOP(!=)

  DEFINE_UNOP(-)
  DEFINE_UNOP(~)
  DEFINE_UNOP(!)

#undef DEFINE_BINOP
#undef DEFINE_UNOP
};

using float16_t = _Float16;
using float32_t = float;
using float64_t = double;

using u8vec2 = Vector<std::uint8_t, 2>;
using u8vec3 = Vector<std::uint8_t, 3>;
using u8vec4 = Vector<std::uint8_t, 4>;
using i8vec2 = Vector<std::int8_t, 2>;
using i8vec3 = Vector<std::int8_t, 3>;
using i8vec4 = Vector<std::int8_t, 4>;

using u16vec2 = Vector<std::uint16_t, 2>;
using u16vec3 = Vector<std::uint16_t, 3>;
using u16vec4 = Vector<std::uint16_t, 4>;
using i16vec2 = Vector<std::int16_t, 2>;
using i16vec3 = Vector<std::int16_t, 3>;
using i16vec4 = Vector<std::int16_t, 4>;

using u32vec2 = Vector<std::uint32_t, 2>;
using u32vec3 = Vector<std::uint32_t, 3>;
using u32vec4 = Vector<std::uint32_t, 4>;
using i32vec2 = Vector<std::int32_t, 2>;
using i32vec3 = Vector<std::int32_t, 3>;
using i32vec4 = Vector<std::int32_t, 4>;

using u64vec2 = Vector<std::uint64_t, 2>;
using u64vec3 = Vector<std::uint64_t, 3>;
using u64vec4 = Vector<std::uint64_t, 4>;
using i64vec2 = Vector<std::int64_t, 2>;
using i64vec3 = Vector<std::int64_t, 3>;
using i64vec4 = Vector<std::int64_t, 4>;

using f32vec2 = Vector<float32_t, 2>;
using f32vec3 = Vector<float32_t, 3>;
using f32vec4 = Vector<float32_t, 4>;
using f64vec2 = Vector<float64_t, 2>;
using f64vec3 = Vector<float64_t, 3>;
using f64vec4 = Vector<float64_t, 4>;

using f16vec2 = Vector<float16_t, 2>;
using f16vec3 = Vector<float16_t, 3>;
using f16vec4 = Vector<float16_t, 4>;

using bvec2 = Vector<bool, 2>;
using bvec3 = Vector<bool, 3>;
using bvec4 = Vector<bool, 4>;
} // namespace shader
