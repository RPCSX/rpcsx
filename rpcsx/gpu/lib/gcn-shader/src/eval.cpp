#include "eval.hpp"
#include "dialect.hpp"
#include "ir.hpp"
#include <cmath>
#include <type_traits>

using namespace shader;

template <typename T>
constexpr bool invokeWithType(ir::Value type, T &&invoke) {
  if (type == ir::spv::OpTypeBool) {
    invoke(bool{});
    return true;
  }

  if (type == ir::spv::OpTypeInt) {
    auto isSigned = *type.getOperand(1).getAsInt32();

    switch (*type.getOperand(0).getAsInt32()) {
    case 8:
      if (isSigned) {
        invoke(std::int8_t{});
        return true;
      }
      invoke(std::uint8_t{});
      return true;

    case 16:
      if (isSigned) {
        invoke(std::int16_t{});
        return true;
      }
      invoke(std::uint16_t{});
      return true;

    case 32:
      if (isSigned) {
        invoke(std::int32_t{});
        return true;
      }
      invoke(std::uint32_t{});
      return true;

    case 64:
      if (isSigned) {
        invoke(std::int64_t{});
        return true;
      }
      invoke(std::uint64_t{});
      return true;
    }

    return false;
  }

  if (type == ir::spv::OpTypeFloat) {
    switch (*type.getOperand(0).getAsInt32()) {
    case 16:
      invoke(shader::float16_t{});
      return true;

    case 32:
      invoke(shader::float32_t{});
      return true;

    case 64:
      invoke(shader::float64_t{});
      return true;
    }

    return false;
  }

  if (type == ir::spv::OpTypeVector) {
    return invokeWithType(type.getOperand(0).getAsValue(),
                          std::forward<T>(invoke));
  }

  return false;
}

static constexpr std::size_t getIrTypeIndex(ir::Value type) {
  std::size_t result = 0;

  invokeWithType(type, [&](auto type) {
    result = eval::Value::getTypeIndex<decltype(type)>();
  });

  return result;
}

static constexpr std::size_t getIrTypeConstituents(ir::Value type) {
  if (type == ir::spv::OpTypeVector) {
    return *type.getOperand(1).getAsInt32();
  }

  return 1;
}

eval::Value
eval::Value::compositeConstruct(ir::Value type,
                                std::span<const eval::Value> constituents) {
  if (getIrTypeConstituents(type) != constituents.size()) {
    return {};
  }

  auto typeIndex = getIrTypeIndex(type);

  eval::Value result;
  for (auto &elem : constituents) {
    result.add(elem);
  }

  if (result.index() != typeIndex) {
    return {};
  }

  return result;
}

eval::Value eval::Value::compositeExtract(const Value &index) const {
  auto optIndexInt = index.zExtScalar();
  if (!optIndexInt) {
    return {};
  }

  auto indexInt = *optIndexInt;

  if (indexInt >= size()) {
    return {};
  }

  eval::Value result;
  result.mTypeIndex = mTypeIndex;
  result.mCount = 1;
  std::memcpy(result.mData, mData + indexInt * getConstituentSize(),
              kMaxTypeSize);
  return result;
}

eval::Value eval::Value::isNan() const {
  return visit([](auto value) {
    eval::Value result;

    if constexpr (std::is_floating_point_v<
                      typename decltype(value)::value_type>) {
      for (std::size_t i = 0; i < value.size(); ++i) {
        result.add(std::isnan(value[i]));
      }
    }

    return result;
  });
}

eval::Value eval::Value::isInf() const {
  return visit([](auto value) {
    eval::Value result;

    if constexpr (std::is_floating_point_v<
                      typename decltype(value)::value_type>) {
      for (std::size_t i = 0; i < value.size(); ++i) {
        result.add(std::isinf(value[i]));
      }
    }

    return result;
  });
}

eval::Value eval::Value::isFinite() const {
  return visit([](auto value) {
    eval::Value result;

    if constexpr (std::is_floating_point_v<
                      typename decltype(value)::value_type>) {
      for (std::size_t i = 0; i < value.size(); ++i) {
        result.add(std::isfinite(value[i]));
      }
    }

    return result;
  });
}

eval::Value eval::Value::makeUnsigned() const {
  return visit([](auto value) {
    eval::Value result;
    using value_type = typename decltype(value)::value_type;

    if constexpr (std::is_integral_v<value_type> &&
                  !std::is_same_v<value_type, bool>) {
      for (std::size_t i = 0; i < value.size(); ++i) {
        result.add(static_cast<std::make_unsigned_t<value_type>>(value[i]));
      }
    }

    return result;
  });
}
eval::Value eval::Value::makeSigned() const {
  return visit([](auto value) {
    eval::Value result;
    using value_type = typename decltype(value)::value_type;

    if constexpr (std::is_integral_v<value_type> &&
                  !std::is_same_v<value_type, bool>) {
      for (std::size_t i = 0; i < value.size(); ++i) {
        result.add(static_cast<std::make_signed_t<value_type>>(value[i]));
      }
    }

    return result;
  });
}

eval::Value eval::Value::all() const {
  return visit([](auto value) {
    if constexpr (std::is_same_v<typename decltype(value)::value_type, bool>) {
      for (std::size_t i = 0; i < value.size(); ++i) {
        if (!value[i]) {
          return eval::Value(false);
        }
      }
      return eval::Value(true);
    }

    return eval::Value();
  });
}

eval::Value eval::Value::any() const {
  return visit([](auto value) {
    if constexpr (std::is_same_v<typename decltype(value)::value_type, bool>) {
      for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i]) {
          return eval::Value(true);
        }
      }
      return eval::Value(false);
    }

    return eval::Value();
  });
}

eval::Value eval::Value::select(const Value &trueValue,
                                const Value &falseValue) const {
  auto optCond = as<bool>();
  if (!optCond) {
    return {};
  }

  auto cond = *optCond;
  return cond ? trueValue : falseValue;
}

eval::Value eval::Value::iConvert(ir::Value type, bool isSigned) const {
  eval::Value result;

  (isSigned ? makeSigned() : makeUnsigned()).visit([&](auto value) {
    invokeWithType(type, [&](auto type) {
      if constexpr (std::is_integral_v<decltype(type)> &&
                    !std::is_same_v<bool, decltype(type)> &&
                    std::is_integral_v<typename decltype(value)::value_type> &&
                    !std::is_same_v<bool,
                                    typename decltype(value)::value_type>) {
        for (auto item : value) {
          result.add(static_cast<decltype(type)>(item));
        }
      }
    });
  });

  return result;
}
eval::Value eval::Value::fConvert(ir::Value type) const {
  eval::Value result;

  visit([&](auto value) {
    invokeWithType(type, [&](auto type) {
      if constexpr (std::is_floating_point_v<decltype(type)> &&
                    std::is_floating_point_v<
                        typename decltype(value)::value_type>) {
        for (auto item : value) {
          result.add(static_cast<decltype(type)>(item));
        }
      }
    });
  });

  return result;
}

eval::Value eval::Value::bitcast(ir::Value type) const {
  eval::Value result;

  auto resultTypeElemCount = getIrTypeConstituents(type);
  visit([&](auto value) {
    invokeWithType(type, [&](auto resultType) {
      if (value.size_bytes() == sizeof(resultType) * resultTypeElemCount) {
        result.mTypeIndex = getIrTypeIndex(type);
        result.mCount = resultTypeElemCount;
        std::memcpy(result.mData, value.data(), value.size_bytes());
      }
    });
  });

  return result;
}

std::optional<std::uint64_t> eval::Value::zExtScalar() const {
  if (empty() || size() != 1) {
    return {};
  }

  return makeUnsigned().visit([](auto value) -> std::optional<std::uint64_t> {
    if constexpr (std::is_integral_v<typename decltype(value)::value_type>) {
      return static_cast<std::uint64_t>(value[0]);
    } else {
      return {};
    }
  });
}

std::optional<std::int64_t> eval::Value::sExtScalar() const {
  if (empty() || size() != 1) {
    return {};
  }

  return makeSigned().visit([](auto value) -> std::optional<std::int64_t> {
    if constexpr (std::is_integral_v<typename decltype(value)::value_type>) {
      return static_cast<std::int64_t>(value[0]);
    } else {
      return {};
    }
  });
}

#define DEFINE_BINARY_OP(OP)                                                   \
  eval::Value eval::Value::operator OP(const Value &rhs) const {               \
    if (index() != rhs.index() || size() != rhs.size()) {                      \
      return {};                                                               \
    }                                                                          \
    eval::Value result;                                                        \
    visit([&](auto lhsValues) {                                                \
      rhs.visit([&](auto rhsValues) {                                          \
        if constexpr (requires { lhsValues[0] OP rhsValues[0]; }) {            \
          if constexpr (std::is_same_v<decltype(lhsValues[0]),                 \
                                       decltype(rhsValues[0])>) {              \
            for (std::size_t i = 0; i < lhsValues.size(); ++i) {               \
              result.add(lhsValues[i] OP rhsValues[i]);                        \
            }                                                                  \
          }                                                                    \
        }                                                                      \
      });                                                                      \
    });                                                                        \
    return result;                                                             \
  }

#define DEFINE_UNARY_OP(OP)                                                    \
  eval::Value eval::Value::operator OP() const {                               \
    eval::Value result;                                                        \
    visit([&](auto values) {                                                   \
      if constexpr (requires { OP values[0]; }) {                              \
        for (std::size_t i = 0; i < values.size(); ++i) {                      \
          result.add(OP values[i]);                                            \
        }                                                                      \
      }                                                                        \
    });                                                                        \
    return result;                                                             \
  }

DEFINE_BINARY_OP(+);
DEFINE_BINARY_OP(-);
DEFINE_BINARY_OP(*);
DEFINE_BINARY_OP(/);
DEFINE_BINARY_OP(%);
DEFINE_BINARY_OP(&);
DEFINE_BINARY_OP(|);
DEFINE_BINARY_OP(^);
DEFINE_BINARY_OP(>>);
DEFINE_BINARY_OP(<<);
DEFINE_BINARY_OP(&&);
DEFINE_BINARY_OP(||);
DEFINE_BINARY_OP(<);
DEFINE_BINARY_OP(>);
DEFINE_BINARY_OP(<=);
DEFINE_BINARY_OP(>=);
DEFINE_BINARY_OP(==);
DEFINE_BINARY_OP(!=);

DEFINE_UNARY_OP(-);
DEFINE_UNARY_OP(~);
DEFINE_UNARY_OP(!);
