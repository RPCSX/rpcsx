#pragma once

#include "Vector.hpp"
#include "ir/Value.hpp"
#include "rx/align.hpp"
#include "rx/die.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

namespace shader::eval {
struct Value {
  using Types = std::tuple<std::nullptr_t, bool, int8_t, int16_t, int32_t,
                           int64_t, uint8_t, uint16_t, uint32_t, uint64_t,
                           float16_t, float32_t, float64_t>;

  static constexpr auto kMaxElementCount = 32;
  static constexpr auto kMaxTypeAlignment = [] {
    auto impl = []<std::size_t... I>(std::index_sequence<I...>) {
      std::size_t result = 1;
      ((result = std::max(alignof(std::tuple_element_t<I, Types>), result)),
       ...);
      return result;
    };
    return impl(std::make_index_sequence<std::tuple_size_v<Types>>{});
  }();
  static constexpr auto kMaxTypeSize = [] {
    auto impl = []<std::size_t... I>(std::index_sequence<I...>) {
      std::size_t result = 1;
      ((result = std::max(sizeof(std::tuple_element_t<I, Types>), result)),
       ...);
      return result;
    };
    return rx::alignUp(
        impl(std::make_index_sequence<std::tuple_size_v<Types>>{}),
        kMaxTypeAlignment);
  }();

  template <typename T> static consteval std::size_t getTypeIndex() {
    auto impl = []<std::size_t... I>(std::index_sequence<I...>) {
      std::size_t result = -1;
      ((result =
            std::is_same_v<T, std::tuple_element_t<I, Types>> ? I : result),
       ...);
      return result;
    };
    return impl(std::make_index_sequence<std::tuple_size_v<Types>>{});
  }

  static constexpr auto StorageSize = std::tuple_size_v<Types>;

  template <typename T, typename RT = std::invoke_result_t<
                            T, std::span<std::tuple_element_t<0, Types>>>>
  RT visit(T &&cb) const {
    static constexpr auto table = [] {
      std::array<RT (*)(void *cb, const char *data, std::uint32_t count),
                 std::tuple_size_v<Types>>
          result;

      auto impl = [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((result[I] = [](void *cb, const char *data,
                         std::uint32_t count) -> RT {
           return (*reinterpret_cast<T *>(cb))(std::span(
               reinterpret_cast<const std::tuple_element_t<I, Types> *>(data),
               count));
         }),
         ...);
      };

      impl(std::make_index_sequence<std::tuple_size_v<Types>>{});

      return result;
    }();

    return table[mTypeIndex](&cb, mData, mCount);
  }

  [[nodiscard]] std::size_t getConstituentSize() const {
    return visit([](auto values) -> std::size_t {
      if constexpr (std::is_same_v<decltype(values[0]), std::nullptr_t>) {
        return 0;
      } else {
        return sizeof(values[0]);
      }
    });
  }

  explicit operator bool() const { return !empty(); }
  [[nodiscard]] bool empty() const {
    return mCount == 0 || mTypeIndex == getTypeIndex<std::nullptr_t>();
  }
  [[nodiscard]] std::size_t size() const { return mCount; }

  Value() = default;

  template <typename FT, typename... T>
    requires(getTypeIndex<FT>() < std::tuple_size_v<Types> &&
             (std::is_same_v<FT, T> && ...))
  Value(FT firstValue, T... value) {
    add(firstValue);
    (add(value), ...);
  }

  void add(const Value &value) {
    if (value.mCount != 1) {
      mTypeIndex = getTypeIndex<std::nullptr_t>();
      mCount = 1;
      return;
    }

    if (mCount == 0) {
      mTypeIndex = value.mTypeIndex;
      mCount = 1;
      std::memcpy(mData, value.mData, kMaxTypeSize);
      return;
    }

    rx::dieIf(mCount >= kMaxElementCount, "storage too small");

    if (mTypeIndex != value.mTypeIndex) {
      mTypeIndex = getTypeIndex<std::nullptr_t>();
      mCount = 1;
      return;
    }

    auto index = mCount++;
    auto elemSize = getConstituentSize();
    std::memcpy(mData + elemSize * index, value.mData, kMaxTypeSize);
  }

  template <typename T>
    requires(getTypeIndex<T>() < std::tuple_size_v<Types>)
  void add(T value) {
    if (mCount == 0) {
      mTypeIndex = getTypeIndex<T>();
      mCount = 1;
      *getData<T>() = value;
      return;
    }

    rx::dieIf(mCount >= kMaxElementCount, "storage too small");

    if (mTypeIndex != getTypeIndex<T>()) {
      mTypeIndex = getTypeIndex<std::nullptr_t>();
      mCount = 1;
      return;
    }

    getData<T>()[mCount++] = value;
  }

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
    requires(getTypeIndex<T>() < std::tuple_size_v<Types>)
  [[nodiscard]] const T &get(std::size_t index = 0) const {
    rx::dieIf(mTypeIndex != getTypeIndex<T>(),
              "eval::Value::get(): invalid type");
    rx::dieIf(index >= std::tuple_size_v<Types>,
              "eval::Value::get(): invalid index");
    return getData<T>()[index];
  }

  template <auto I>
    requires(I < std::tuple_size_v<Types>)
  [[nodiscard]] const std::tuple_element_t<I, Types> &
  get(std::size_t index = 0) const {
    rx::dieIf(mTypeIndex != I, "eval::Value::get(): invalid type");
    rx::dieIf(index >= std::tuple_size_v<Types>,
              "eval::Value::get(): invalid index");
    return getData<std::tuple_element_t<I, Types>>()[index];
  }

  template <typename T>
    requires(getTypeIndex<T>() < std::tuple_size_v<Types>)
  [[nodiscard]] std::optional<T> as(std::size_t index = 0) const {
    rx::dieIf(index >= std::tuple_size_v<Types>,
              "eval::Value::as(): invalid index");
    if (mTypeIndex == getTypeIndex<T>()) {
      return getData<T>()[index];
    }

    return std::nullopt;
  }

  template <typename T>
    requires(getTypeIndex<T>() < std::tuple_size_v<Types>)
  [[nodiscard]] bool is() const {
    return mTypeIndex == getTypeIndex<T>();
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

  std::size_t index() const { return mTypeIndex; }

private:
  template <typename T> T *getData() { return reinterpret_cast<T *>(mData); }
  template <typename T> const T *getData() const {
    return reinterpret_cast<const T *>(mData);
  }

  std::uint32_t mTypeIndex = 0;
  std::uint32_t mCount = 0;
  alignas(kMaxTypeAlignment) char mData[kMaxTypeSize * kMaxElementCount];
};
} // namespace shader::eval
