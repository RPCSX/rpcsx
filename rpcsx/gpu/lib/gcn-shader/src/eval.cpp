#include "eval.hpp"
#include "dialect.hpp"
#include "ir.hpp"
#include <cmath>
#include <concepts>

using namespace shader;

template <typename Cond, typename... Args> consteval bool testVisitCond() {
  if constexpr (std::is_same_v<Cond, void>) {
    return true;
  } else {
    return Cond{}(std::remove_cvref_t<Args>{}...);
  }
};

template <typename Cond, std::size_t U> consteval bool testVisitCond() {
  if constexpr (U >= eval::Value::StorageSize) {
    return false;
  } else if constexpr (std::is_same_v<Cond, void>) {
    return true;
  } else {
    return Cond{}(std::variant_alternative_t<U, eval::Value::Storage>{});
  }
};

template <typename Cond = void, size_t I = 0>
constexpr eval::Value visitImpl(const eval::Value &variant, auto &&fn) {

#define DEFINE_CASE(N)                                                         \
  case I + N:                                                                  \
    if constexpr (testVisitCond<Cond, I + N>()) {                              \
      return std::forward<decltype(fn)>(fn)(std::get<I + N>(variant.storage)); \
    } else {                                                                   \
      return {};                                                               \
    }

  switch (variant.storage.index()) {
    DEFINE_CASE(0);
    DEFINE_CASE(1);
    DEFINE_CASE(2);
    DEFINE_CASE(3);
    DEFINE_CASE(4);
    DEFINE_CASE(5);
    DEFINE_CASE(6);
    DEFINE_CASE(7);
    DEFINE_CASE(8);
    DEFINE_CASE(9);
    DEFINE_CASE(10);
    DEFINE_CASE(11);
    DEFINE_CASE(12);
    DEFINE_CASE(13);
    DEFINE_CASE(14);
    DEFINE_CASE(15);
    DEFINE_CASE(16);
    DEFINE_CASE(17);
    DEFINE_CASE(18);
    DEFINE_CASE(19);
    DEFINE_CASE(20);
    DEFINE_CASE(21);
    DEFINE_CASE(22);
    DEFINE_CASE(23);
    DEFINE_CASE(24);
    DEFINE_CASE(25);
    DEFINE_CASE(26);
    DEFINE_CASE(27);
    DEFINE_CASE(28);
    DEFINE_CASE(29);
    DEFINE_CASE(30);
    DEFINE_CASE(31);
    DEFINE_CASE(32);
    DEFINE_CASE(33);
    DEFINE_CASE(34);
    DEFINE_CASE(35);
    DEFINE_CASE(36);
    DEFINE_CASE(37);
    DEFINE_CASE(38);
    DEFINE_CASE(39);
    DEFINE_CASE(40);
    DEFINE_CASE(41);
    DEFINE_CASE(42);
    DEFINE_CASE(43);
    DEFINE_CASE(44);
    DEFINE_CASE(45);
    DEFINE_CASE(46);
    DEFINE_CASE(47);
    DEFINE_CASE(48);
    DEFINE_CASE(49);
    DEFINE_CASE(50);
    DEFINE_CASE(51);
    DEFINE_CASE(52);
    DEFINE_CASE(53);
    DEFINE_CASE(54);
    DEFINE_CASE(55);
    DEFINE_CASE(56);
    DEFINE_CASE(57);
    DEFINE_CASE(58);
    DEFINE_CASE(59);
    DEFINE_CASE(60);
    DEFINE_CASE(61);
    DEFINE_CASE(62);
    DEFINE_CASE(63);
  }
#undef DEFINE_CASE

  constexpr auto NextIndex = I + 64;

  if constexpr (NextIndex < eval::Value::StorageSize) {
    return visitImpl<Cond, NextIndex>(std::forward<decltype(fn)>(fn),
                                      std::forward<decltype(variant)>(variant));
  }

  return {};
}

template <typename Cond = void, typename Cb>
constexpr eval::Value visitScalarType(ir::Value type, Cb &&cb)
  requires requires {
    { std::forward<Cb>(cb)(int{}) } -> std::same_as<eval::Value>;
  }
{
  auto invoke = [&](auto type) -> eval::Value {
    if constexpr (testVisitCond<Cond, std::remove_cvref_t<decltype(type)>>()) {
      return std::forward<Cb>(cb)(type);
    }
    return {};
  };

  if (type == ir::spv::OpTypeBool) {
    return invoke(bool{});
  }

  if (type == ir::spv::OpTypeInt) {
    auto isSigned = *type.getOperand(1).getAsInt32();

    switch (*type.getOperand(0).getAsInt32()) {
    case 8:
      if (isSigned) {
        return invoke(std::int8_t{});
      }
      return invoke(std::uint8_t{});

    case 16:
      if (isSigned) {
        return invoke(std::int16_t{});
      }
      return invoke(std::uint16_t{});

    case 32:
      if (isSigned) {
        return invoke(std::int32_t{});
      }
      return invoke(std::uint32_t{});

    case 64:
      if (isSigned) {
        return invoke(std::int64_t{});
      }
      return invoke(std::uint64_t{});
    }

    return {};
  }

  if (type == ir::spv::OpTypeFloat) {
    switch (*type.getOperand(0).getAsInt32()) {
    case 16:
      return invoke(shader::float16_t{});

    case 32:
      return invoke(shader::float32_t{});

    case 64:
      return invoke(shader::float64_t{});
    }

    return {};
  }

  return {};
}

template <typename Cond = void, typename Cb>
constexpr eval::Value visitType(ir::Value type, Cb &&cb)
  requires requires {
    { std::forward<Cb>(cb)(int{}) } -> std::same_as<eval::Value>;
  }
{
  if (type == ir::spv::OpTypeInt || type == ir::spv::OpTypeFloat ||
      type == ir::spv::OpTypeBool) {
    return visitScalarType<Cond>(type, cb);
  }

  auto invoke = [&](auto type) -> eval::Value {
    if constexpr (testVisitCond<Cond, std::remove_cvref_t<decltype(type)>>()) {
      return std::forward<Cb>(cb)(type);
    } else {
      return {};
    }
  };

  if (type == ir::spv::OpTypeVector) {
    switch (*type.getOperand(1).getAsInt32()) {
    case 2:
      return visitScalarType(
          type.getOperand(0).getAsValue(),
          [&]<typename T>(T) { return invoke(shader::Vector<T, 2>{}); });

    case 3:
      return visitScalarType(
          type.getOperand(0).getAsValue(),
          [&]<typename T>(T) { return invoke(shader::Vector<T, 3>{}); });

    case 4:
      return visitScalarType(
          type.getOperand(0).getAsValue(),
          [&]<typename T>(T) { return invoke(shader::Vector<T, 4>{}); });
    }

    return {};
  }

  return {};
}

template <typename Cond = void, typename Cb>
eval::Value visit(const eval::Value &value, Cb &&cb) {
  using VisitCond = decltype([](auto &&storage) {
    using T = std::remove_cvref_t<decltype(storage)>;
    if constexpr (std::is_same_v<T, std::nullptr_t>) {
      return false;
    } else {
      return testVisitCond<Cond, T>();
    }
  });

  return visitImpl<VisitCond>(value, std::forward<Cb>(cb));
}

template <typename Cb>
eval::Value visit2(auto &&cond, const eval::Value &value, Cb &&cb) {
  if constexpr (cond()) {
    return visitImpl(value, std::forward<Cb>(cb));
  } else {
    return {};
  }
}

template <typename ValueCond = void, typename TypeVisitCond = void,
          typename TypeValueVisitCond = void, typename Cb>
eval::Value visitWithType(const eval::Value &value, ir::Value type, Cb &&cb) {
  using ValueVisitCond = decltype([](auto storage) {
    if constexpr (std::is_same_v<decltype(storage), std::nullptr_t>) {
      return false;
    } else {
      return testVisitCond<ValueCond, decltype(storage)>();
    }
  });

  return visitImpl<ValueVisitCond>(value, [&](auto &&value) -> eval::Value {
    return visitType<TypeVisitCond>(type, [&](auto type) -> eval::Value {
      if constexpr (testVisitCond<TypeValueVisitCond, decltype(type),
                                  decltype(value)>()) {
        return std::forward<Cb>(cb)(type, value);
      } else {
        return {};
      }
    });
  });
}

namespace {
template <typename T> struct ComponentTypeImpl {
  using type = T;
};

template <typename T, std::size_t N> struct ComponentTypeImpl<Vector<T, N>> {
  using type = T;
};

template <typename T, std::size_t N>
struct ComponentTypeImpl<std::array<T, N>> {
  using type = T;
};

template <typename T> struct MakeSignedImpl {
  using type = std::make_signed_t<T>;
};

template <typename T, std::size_t N> struct MakeSignedImpl<Vector<T, N>> {
  using type = Vector<std::make_signed_t<T>, N>;
};
template <typename T> struct MakeUnsignedImpl {
  using type = std::make_unsigned_t<T>;
};

template <typename T, std::size_t N> struct MakeUnsignedImpl<Vector<T, N>> {
  using type = Vector<std::make_unsigned_t<T>, N>;
};
} // namespace

template <typename T> using ComponentType = typename ComponentTypeImpl<T>::type;
template <typename T> using MakeSigned = typename MakeSignedImpl<T>::type;
template <typename T> using MakeUnsigned = typename MakeUnsignedImpl<T>::type;

template <typename> constexpr std::size_t Components = 1;
template <typename T, std::size_t N>
constexpr std::size_t Components<Vector<T, N>> = N;
template <typename T, std::size_t N>
constexpr std::size_t Components<std::array<T, N>> = N;

template <typename> constexpr bool IsArray = false;
template <typename T, std::size_t N>
constexpr bool IsArray<std::array<T, N>> = true;

eval::Value
eval::Value::compositeConstruct(ir::Value type,
                                std::span<const eval::Value> constituents) {
  using Cond =
      decltype([](auto type) { return Components<decltype(type)> > 1; });

  return visitType<Cond>(type, [&](auto type) -> Value {
    constexpr std::size_t N = Components<decltype(type)>;
    if (N != constituents.size()) {
      return {};
    }

    decltype(type) result;

    for (std::size_t i = 0; i < N; ++i) {
      if (auto value = constituents[i].as<ComponentType<decltype(type)>>()) {
        result[i] = *value;
      } else {
        return {};
      }
    }

    return result;
  });
}

eval::Value eval::Value::compositeExtract(const Value &index) const {
  using Cond =
      decltype([](auto type) { return Components<decltype(type)> > 1; });

  auto optIndexInt = index.zExtScalar();
  if (!optIndexInt) {
    return {};
  }

  auto indexInt = *optIndexInt;

  return visit<Cond>(*this, [&](auto &&value) -> Value {
    using ValueType = std::remove_cvref_t<decltype(value)>;
    constexpr std::size_t N = Components<ValueType>;

    if (indexInt >= N) {
      return {};
    }

    return value[indexInt];
  });
}

eval::Value eval::Value::isNan() const {
  using Cond = decltype([](auto type) {
    return std::is_floating_point_v<ComponentType<decltype(type)>> &&
           !IsArray<decltype(type)>;
  });

  return visit<Cond>(*this, [](auto &&value) -> Value {
    constexpr std::size_t N = Components<std::remove_cvref_t<decltype(value)>>;

    if constexpr (N == 1) {
      return std::isnan(value);
    } else {
      Vector<bool, N> result;
      for (std::size_t i = 0; i < N; ++i) {
        result[i] = std::isnan(value[i]);
      }
      return result;
    }
  });
}

eval::Value eval::Value::isInf() const {
  using Cond = decltype([](auto type) {
    return std::is_floating_point_v<ComponentType<decltype(type)>> &&
           !IsArray<decltype(type)>;
  });

  return visit<Cond>(*this, [](auto &&value) -> Value {
    constexpr std::size_t N = Components<std::remove_cvref_t<decltype(value)>>;

    if constexpr (N == 1) {
      return std::isinf(value);
    } else {
      Vector<bool, N> result;
      for (std::size_t i = 0; i < N; ++i) {
        result[i] = std::isinf(value[i]);
      }
      return result;
    }
  });
}

eval::Value eval::Value::isFinite() const {
  using Cond = decltype([](auto type) {
    return std::is_floating_point_v<ComponentType<decltype(type)>>;
  });

  return visit<Cond>(*this, [](auto &&value) -> Value {
    constexpr std::size_t N = Components<std::remove_cvref_t<decltype(value)>>;

    if constexpr (N == 1) {
      return std::isfinite(value);
    } else {
      Vector<bool, N> result;
      for (std::size_t i = 0; i < N; ++i) {
        result[i] = std::isfinite(value[i]);
      }
      return result;
    }
  });
}

eval::Value eval::Value::makeUnsigned() const {
  using Cond = decltype([](auto type) {
    return std::is_integral_v<ComponentType<decltype(type)>> &&
           !std::is_same_v<ComponentType<decltype(type)>, bool> &&
           !IsArray<decltype(type)>;
  });

  return visit<Cond>(*this, [](auto &&value) -> Value {
    constexpr std::size_t N = Components<std::remove_cvref_t<decltype(value)>>;
    using T = std::make_unsigned_t<
        ComponentType<std::remove_cvref_t<decltype(value)>>>;

    if constexpr (N == 1) {
      return static_cast<T>(value);
    } else {
      Vector<T, N> result;
      for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<T>(value[i]);
      }
      return result;
    }
  });
}
eval::Value eval::Value::makeSigned() const {
  using Cond = decltype([](auto type) {
    return std::is_integral_v<ComponentType<decltype(type)>> &&
           !std::is_same_v<ComponentType<decltype(type)>, bool> &&
           !IsArray<decltype(type)>;
  });

  return visit<Cond>(*this, [](auto &&value) -> Value {
    constexpr std::size_t N = Components<std::remove_cvref_t<decltype(value)>>;
    using T =
        std::make_signed_t<ComponentType<std::remove_cvref_t<decltype(value)>>>;

    if constexpr (N == 1) {
      return static_cast<T>(value);
    } else {
      Vector<T, N> result;
      for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<T>(value[i]);
      }
      return result;
    }
  });
}

eval::Value eval::Value::all() const {
  using Cond = decltype([](auto type) {
    return std::is_same_v<ComponentType<decltype(type)>, bool> &&
           (Components<decltype(type)> > 1) && !IsArray<decltype(type)>;
  });

  return visit<Cond>(*this, [](auto &&value) {
    constexpr std::size_t N = Components<std::remove_cvref_t<decltype(value)>>;
    for (std::size_t i = 0; i < N; ++i) {
      if (!value[i]) {
        return false;
      }
    }
    return true;
  });
}

eval::Value eval::Value::any() const {
  using Cond = decltype([](auto type) {
    return std::is_same_v<ComponentType<decltype(type)>, bool> &&
           (Components<decltype(type)> > 1) && !IsArray<decltype(type)>;
  });

  return visit<Cond>(*this, [](auto &&value) {
    constexpr std::size_t N = Components<std::remove_cvref_t<decltype(value)>>;
    for (std::size_t i = 0; i < N; ++i) {
      if (value[i]) {
        return true;
      }
    }
    return false;
  });
}

eval::Value eval::Value::select(const Value &trueValue,
                                const Value &falseValue) const {
  using Cond = decltype([](auto type) consteval {
    return std::is_same_v<ComponentType<decltype(type)>, bool> &&
           !IsArray<decltype(type)>;
  });

  return visit<Cond>(*this, [&](auto &&cond) -> Value {
    using CondType = std::remove_cvref_t<decltype(cond)>;
    using TrueCond = decltype([](auto type) consteval {
      return Components<decltype(type)> == Components<CondType>;
    });

    return visit<TrueCond>(trueValue, [&](auto &&trueValue) {
      using TrueValue = std::remove_cvref_t<decltype(trueValue)>;
      using FalseCond = decltype([](auto type) {
        return std::is_same_v<TrueValue, std::remove_cvref_t<decltype(type)>>;
      });

      return visit(falseValue, [&](auto &&falseValue) -> Value {
        if constexpr (std::is_same_v<TrueValue, std::remove_cvref_t<
                                                    decltype(falseValue)>>) {
          constexpr std::size_t N = Components<CondType>;

          if constexpr (N == 1) {
            return cond ? trueValue : falseValue;
          } else {
            Vector<bool, N> result;
            for (std::size_t i = 0; i < N; ++i) {
              result[i] = cond[i] ? trueValue[i] : falseValue[i];
            }
            return result;
          }
        } else {
          return {};
        }
      });
    });
  });
}

eval::Value eval::Value::iConvert(ir::Value type, bool isSigned) const {
  using Cond = decltype([](auto type) {
    using Type = std::remove_cvref_t<decltype(type)>;

    return std::is_integral_v<ComponentType<Type>> &&
           !std::is_same_v<bool, ComponentType<Type>> &&
           !IsArray<decltype(type)>;
  });

  using PairCond = decltype([](auto lhs, auto rhs) {
    using Lhs = decltype(lhs);
    using Rhs = decltype(rhs);

    return !std::is_same_v<Lhs, Rhs> && Components<Lhs> == Components<Rhs>;
  });

  return visitWithType<Cond, Cond, PairCond>(
      *this, type, [&](auto type, auto &&value) -> Value {
        using Type = std::remove_cvref_t<decltype(type)>;
        using ValueType = std::remove_cvref_t<decltype(value)>;
        if (isSigned) {
          return static_cast<Type>(static_cast<MakeSigned<ValueType>>(value));
        } else {
          return static_cast<Type>(static_cast<MakeUnsigned<ValueType>>(value));
        }
      });
}
eval::Value eval::Value::fConvert(ir::Value type) const {
  using Cond = decltype([](auto type) {
    return std::is_floating_point_v<ComponentType<decltype(type)>> &&
           !IsArray<decltype(type)>;
  });

  using PairCond = decltype([](auto lhs, auto rhs) {
    using Lhs = decltype(lhs);
    using Rhs = decltype(rhs);

    return !std::is_same_v<Lhs, Rhs> && Components<Lhs> == Components<Rhs>;
  });

  return visitWithType<void, void, PairCond>(
      *this, type, [&](auto type, auto &&value) -> Value {
        using Type = std::remove_cvref_t<decltype(type)>;
        return static_cast<Type>(value);
      });
}

eval::Value eval::Value::bitcast(ir::Value type) const {
  using Cond = decltype([](auto type, auto value) {
    using Type = std::remove_cvref_t<decltype(type)>;

    return sizeof(type) == sizeof(value);
  });

  return visitWithType<void, void, Cond>(
      *this, type, [](auto type, auto &&value) -> Value {
        return std::bit_cast<decltype(type)>(value);
      });
}

std::optional<std::uint64_t> eval::Value::zExtScalar() const {
  using Cond = decltype([](auto type) {
    return std::is_integral_v<ComponentType<decltype(type)>> &&
           !std::is_same_v<ComponentType<decltype(type)>, bool> &&
           Components<decltype(type)> == 1 && !IsArray<decltype(type)>;
  });

  auto result = visit<Cond>(*this, [&](auto value) -> Value {
    return static_cast<std::uint64_t>(
        static_cast<MakeUnsigned<decltype(value)>>(value));
  });

  if (result) {
    return result.as<std::uint64_t>();
  }

  return {};
}

std::optional<std::int64_t> eval::Value::sExtScalar() const {
  using Cond = decltype([](auto type) {
    return std::is_integral_v<ComponentType<decltype(type)>> &&
           !std::is_same_v<ComponentType<decltype(type)>, bool> &&
           Components<decltype(type)> == 1 && !IsArray<decltype(type)>;
  });

  auto result = visit<Cond>(*this, [&](auto value) -> Value {
    return static_cast<std::int64_t>(
        static_cast<MakeSigned<decltype(value)>>(value));
  });

  if (result) {
    return result.as<std::int64_t>();
  }

  return {};
}

#define DEFINE_BINARY_OP(OP)                                                   \
  eval::Value eval::Value::operator OP(const Value &rhs) const {               \
    using LhsCond = decltype([](auto &&lhs) { return true; });                 \
    return visit<LhsCond>(*this, [&]<typename Lhs>(Lhs &&lhs) -> Value {       \
      using RhsCond = decltype([](auto &&rhs) {                                \
        return requires(Lhs lhs) { static_cast<Value>(lhs OP rhs); };          \
      });                                                                      \
      return visit<RhsCond>(rhs, [&](auto &&rhs) -> Value {                    \
        return static_cast<Value>(lhs OP rhs);                                 \
      });                                                                      \
    });                                                                        \
  }

#define DEFINE_UNARY_OP(OP)                                                    \
  eval::Value eval::Value::operator OP() const {                               \
    using Cond = decltype([](auto rhs) {                                       \
      return requires { static_cast<Value>(OP rhs); };                         \
    });                                                                        \
    return visit<Cond>(*this, [&](auto &&rhs) -> Value {                       \
      return static_cast<Value>(OP rhs);                                       \
    });                                                                        \
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
