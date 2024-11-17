#pragma once

#include "Vector.hpp"
#include "ir/Value.hpp"
#include <array>
#include <cstdint>
#include <variant>

namespace shader::eval {
struct Value {
  using Storage = std::variant<
      std::nullptr_t, std::int8_t, std::int16_t, std::int32_t, std::int64_t,
      std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, float16_t,
      float32_t, float64_t, u8vec2, u8vec3, u8vec4, i8vec2, i8vec3, i8vec4,
      u16vec2, u16vec3, u16vec4, i16vec2, i16vec3, i16vec4, u32vec2, u32vec3,
      u32vec4, i32vec2, i32vec3, i32vec4, u64vec2, u64vec3, u64vec4, i64vec2,
      i64vec3, i64vec4, f32vec2, f32vec3, f32vec4, f64vec2, f64vec3, f64vec4,
      f16vec2, f16vec3, f16vec4, bool, bvec2, bvec3, bvec4,
      std::array<uint32_t, 8>, std::array<std::uint32_t, 16>>;
  static constexpr auto StorageSize = std::variant_size_v<Storage>;
  Storage storage;

  explicit operator bool() const { return !empty(); }
  bool empty() const { return storage.index() == 0; }

  Value() : storage(nullptr) {}

  template <typename T>
  Value(T &&value)
    requires requires { Storage(std::forward<T>(value)); }
      : storage(std::forward<T>(value)) {}

  static Value compositeConstruct(ir::Value type,
                                  std::span<const Value> constituents);
  Value compositeExtract(const Value &index) const;
  // Value compositeInsert(const Value &object, std::size_t index) const;

  Value isNan() const;
  Value isInf() const;
  Value isFinite() const;
  Value makeUnsigned() const;
  Value makeSigned() const;
  Value all() const;
  Value any() const;
  Value select(const Value &trueValue, const Value &falseValue) const;
  Value iConvert(ir::Value type, bool isSigned) const;
  Value sConvert(ir::Value type) const { return iConvert(type, true); }
  Value uConvert(ir::Value type) const { return iConvert(type, false); }
  Value fConvert(ir::Value type) const;
  Value bitcast(ir::Value type) const;
  std::optional<std::uint64_t> zExtScalar() const;
  std::optional<std::int64_t> sExtScalar() const;

  template <typename T>
    requires requires { std::get<T>(storage); }
  T get() const {
    return std::get<T>(storage);
  }

  template <typename T>
    requires requires { std::get<T>(storage); }
  std::optional<T> as() const {
    if (auto result = std::get_if<T>(&storage)) {
      return *result;
    }

    return std::nullopt;
  }

  Value operator+(const Value &rhs) const;
  Value operator-(const Value &rhs) const;
  Value operator*(const Value &rhs) const;
  Value operator/(const Value &rhs) const;
  Value operator%(const Value &rhs) const;
  Value operator&(const Value &rhs) const;
  Value operator|(const Value &rhs) const;
  Value operator^(const Value &rhs) const;
  Value operator>>(const Value &rhs) const;
  Value operator<<(const Value &rhs) const;
  Value operator&&(const Value &rhs) const;
  Value operator||(const Value &rhs) const;
  Value operator<(const Value &rhs) const;
  Value operator>(const Value &rhs) const;
  Value operator<=(const Value &rhs) const;
  Value operator>=(const Value &rhs) const;
  Value operator==(const Value &rhs) const;
  Value operator!=(const Value &rhs) const;

  Value operator-() const;
  Value operator~() const;
  Value operator!() const;
};
} // namespace shader::eval
