#pragma once

#include "../Vector.hpp"
#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace shader::ir {
class NameStorage;
class Context;
struct ValueImpl;
struct Value;
struct NodeImpl;
struct CloneMap;
template <typename ImplT> struct NodeWrapper;
using Node = NodeWrapper<NodeImpl>;

struct Operand {
  using UnderlyingT =
      std::variant<std::nullptr_t, ValueImpl *, std::int64_t, std::int32_t,
                   double, float, bool, std::string>;

  UnderlyingT value{nullptr};

  template <typename T>
    requires(!std::is_integral_v<std::remove_cvref_t<T>> ||
             std::is_same_v<bool, std::remove_cvref_t<T>>)
  Operand(T &&value)
    requires requires { UnderlyingT{std::forward<T>(value)}; }
      : value(std::forward<T>(value)) {}

  template <typename T>
  Operand(T value)
    requires requires {
      requires(std::is_integral_v<std::remove_cvref_t<T>> &&
               !std::is_same_v<bool, T> && sizeof(T) <= sizeof(std::int32_t));
      UnderlyingT{static_cast<std::int32_t>(value)};
    }
      : value(static_cast<std::int32_t>(value)) {}

  template <typename T>
  Operand(T value)
    requires requires {
      requires(std::is_integral_v<std::remove_cvref_t<T>> &&
               sizeof(T) == sizeof(std::int64_t));
      UnderlyingT{static_cast<std::int64_t>(value)};
    }
      : value(static_cast<std::int64_t>(value)) {}

  template <typename T>
    requires(std::is_enum_v<std::remove_cvref_t<T>>)
  Operand(T value) : Operand(std::to_underlying(value)) {}

  template <typename T>
  Operand(T &&value)
    requires requires { Operand(value.impl); }
      : Operand(value.impl) {
    if (value.impl == nullptr) {
      std::abort();
    }
  }

  Operand() = default;
  Operand(const Operand &) = default;
  Operand(Operand &&) = default;
  Operand &operator=(const Operand &) = default;
  Operand &operator=(Operand &&) = default;

  template <typename T>
  Operand &operator=(T &&other)
    requires requires { value = std::forward<T>(other); }
  {
    value = std::forward<T>(other);
    return *this;
  }

  template <typename T> const T *getAs() const {
    if (auto node = std::get_if<T>(&value)) {
      return node;
    }

    return {};
  }

  Value getAsValue() const;

  const std::string *getAsString() const { return getAs<std::string>(); }
  const std::int32_t *getAsInt32() const { return getAs<std::int32_t>(); }
  const std::int64_t *getAsInt64() const { return getAs<std::int64_t>(); }
  const double *getAsDouble() const { return getAs<double>(); }
  const float *getAsFloat() const { return getAs<float>(); }
  const bool *getAsBool() const { return getAs<bool>(); }
  bool isNull() const { return std::get_if<std::nullptr_t>(&value) != nullptr; }
  explicit operator bool() const { return !isNull(); }

  void print(std::ostream &os, NameStorage &ns) const;
  Operand clone(Context &context, CloneMap &map) const;

  std::partial_ordering operator<=>(const Operand &other) const {
    auto result = value.index() <=> other.value.index();
    if (result != 0) {
      return result;
    }

    return std::visit(
        [](auto &&lhs, auto &&rhs) -> std::partial_ordering {
          using lhs_type = std::remove_cvref_t<decltype(lhs)>;
          using rhs_type = std::remove_cvref_t<decltype(rhs)>;
          if constexpr (std::is_same_v<lhs_type, rhs_type>) {
            if constexpr (std::is_same_v<lhs_type, std::nullptr_t>) {
              return std::strong_ordering::equal;
            } else if constexpr (std::is_same_v<lhs_type, float>) {
              return std::bit_cast<std::uint32_t>(lhs) <=>
                     std::bit_cast<std::uint32_t>(rhs);
            } else if constexpr (std::is_same_v<lhs_type, double>) {
              return std::bit_cast<std::uint64_t>(lhs) <=>
                     std::bit_cast<std::uint64_t>(rhs);
            } else {
              return lhs <=> rhs;
            }
          }

          throw;
        },
        value, other.value);
  }

  bool operator==(const Operand &) const = default;
};

struct OperandList : std::vector<Operand> {
  using std::vector<Operand>::vector;
  using std::vector<Operand>::operator=;

  template <typename T>
    requires std::is_enum_v<T>
  void addOperand(T enumValue) {
    addOperand(std::to_underlying(enumValue));
  }

  void addOperand(Operand operand) { push_back(std::move(operand)); }

  const Operand &getOperand(std::size_t i) const { return at(i); }
};
} // namespace shader::ir
