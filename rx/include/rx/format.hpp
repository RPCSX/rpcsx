#pragma once
#include "refl.hpp"
#include <array>
#include <format>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace rx {
namespace detail {
struct StructFieldInfo {
  std::size_t size = 0;
  std::size_t align = 0;
  std::size_t offset = 0;
  std::format_context::iterator (*format)(void *,
                                          std::format_context &ctx) = nullptr;
  std::string_view name;
};

struct StructFieldQuery {
  StructFieldInfo info;

  template <typename T> constexpr operator T() {
    info.size = sizeof(T);
    info.align = alignof(T);
    if constexpr (std::is_default_constructible_v<std::formatter<T>>) {
      info.format =
          [](void *object,
             std::format_context &ctx) -> std::format_context::iterator {
        std::formatter<T> formatter;
        return formatter.format(*static_cast<T *>(object), ctx);
      };
    }

    return {};
  }
};

template <typename StructT, typename T>
std::size_t getFieldOffset(T StructT::*ptr) {
  StructT queryStruct;
  return std::bit_cast<std::byte *>(&(queryStruct.*ptr)) -
         std::bit_cast<std::byte *>(&queryStruct);
}

template <typename> struct StructRuntimeInfo {
  std::unordered_map<std::size_t, std::string_view> fieldInfo;

  template <auto Field> void registerField() {
    fieldInfo[getFieldOffset(Field)] = getNameOf<Field>();
  }

  std::string_view getFieldName(std::size_t offset) {
    if (auto it = fieldInfo.find(offset); it != fieldInfo.end()) {
      return it->second;
    }

    return {};
  }
};

struct VariableRuntimeInfo {
  std::unordered_map<void *, std::string_view> infos;

  std::string_view getVariableName(void *pointer) {
    if (auto it = infos.find(pointer); it != infos.end()) {
      return it->second;
    }

    return {};
  }
};

template <typename> struct UnwrapFieldInfo;

template <typename T, typename StructT> struct UnwrapFieldInfo<T StructT::*> {
  using struct_type = StructT;
  using field_type = T;
};

template <typename StructT> auto &getStructStorage() {
  static StructRuntimeInfo<StructT> structStorage;
  return structStorage;
}

inline auto &getVariableStorage() {
  static VariableRuntimeInfo storage;
  return storage;
}

template <typename StructT, std::size_t N = fieldCount<StructT>>
constexpr auto getConstStructInfo() {
  auto genInfo =
      []<std::size_t... I>(
          std::index_sequence<I...>) -> std::array<StructFieldInfo, N> {
    std::array<StructFieldQuery, N> queries;
    static_cast<void>(StructT{queries[I]...});

    auto result = std::array<StructFieldInfo, N>{queries[I].info...};

    std::size_t nextOffset = 0;
    for (auto &elem : result) {
      elem.offset = (nextOffset + (elem.align - 1)) & ~(elem.align - 1);
      nextOffset = elem.offset + elem.size;
    }

    return result;
  };

  return genInfo(std::make_index_sequence<N>());
}

template <typename StructT> constexpr auto getStructInfo() {
  auto structInfo = getConstStructInfo<StructT>();
  auto &runtimeInfo = getStructStorage<StructT>();
  for (auto &field : structInfo) {
    field.name = runtimeInfo.getFieldName(field.offset);
  }
  return structInfo;
}
} // namespace detail

template <auto Field> void registerField() {
  using Info = detail::UnwrapFieldInfo<decltype(Field)>;
  auto &storage = detail::getStructStorage<typename Info::struct_type>();
  storage.template registerField<Field>();
}

template <auto &&Variable> void registerVariable() {
  auto &storage = detail::getVariableStorage();
  storage.infos[&Variable] = rx::getNameOf<Variable>();
}
} // namespace rx

template <typename T>
  requires(std::is_standard_layout_v<T> && std::is_class_v<T> &&
           rx::fieldCount<T> > 0) &&
          (!requires(T value) { std::begin(value) != std::end(value); })
struct std::formatter<T> {
  constexpr std::format_parse_context::iterator
  parse(std::format_parse_context &ctx) {
    return ctx.begin();
  }

  std::format_context::iterator format(T &s, std::format_context &ctx) const {
    std::format_to(ctx.out(), "{}", rx::getNameOf<T>());
    std::format_to(ctx.out(), "{{");

    auto structInfo = rx::detail::getStructInfo<T>();
    auto bytes = reinterpret_cast<std::byte *>(&s);
    for (std::size_t i = 0; i < rx::fieldCount<T>; ++i) {
      if (i != 0) {
        std::format_to(ctx.out(), ", ");
      }

      if (!structInfo[i].name.empty()) {
        std::format_to(ctx.out(), ".{} = ", structInfo[i].name);
      }

      structInfo[i].format(bytes + structInfo[i].offset, ctx);
    }

    std::format_to(ctx.out(), "}}");
    return ctx.out();
  }
};

template <typename T>
  requires(std::is_enum_v<T> && rx::fieldCount<T> > 0)
struct std::formatter<T> {
  constexpr std::format_parse_context::iterator
  parse(std::format_parse_context &ctx) {
    return ctx.begin();
  }

  std::format_context::iterator format(T value,
                                       std::format_context &ctx) const {
    auto getFieldName =
        []<std::size_t... I>(std::underlying_type_t<T> value,
                             std::index_sequence<I...>) -> std::string {
      std::string_view result;
      ((value == I ? ((result = rx::getNameOf<static_cast<T>(I)>()), 0) : 0),
       ...);

      if (!result.empty()) {
        return std::string(result);
      }

      return std::format("{}", value);
    };

    auto queryUnknownField =
        []<std::int64_t Offset, std::int64_t... I>(
            std::underlying_type_t<T> value,
            std::integral_constant<std::int64_t, Offset>,
            std::integer_sequence<std::int64_t, I...>) -> std::string {
      std::string_view result;
      auto queryIndex = [&]<std::int64_t Index>(
                            std::integral_constant<std::int64_t, Index>,
                            std::int64_t value) {
        if (value == Index) {
          if constexpr (requires { rx::getNameOf<static_cast<T>(Index)>(); }) {
            result = rx::getNameOf<static_cast<T>(Index)>();
          }
        }
      };

      if (value < 0) {
        (queryIndex(std::integral_constant<std::int64_t, -(I + Offset)>{}, value), ...);
      } else {
        (queryIndex(std::integral_constant<std::int64_t, I + Offset>{}, value), ...);
      }

      if (!result.empty()) {
        return std::string(result);
      }

      return std::format("{}", value);
    };

    std::string fieldName;

    // FIXME: requires C++23
    // auto underlying = std::to_underlying(value);
    auto underlying = static_cast<int>(value);

    if (underlying < 0) {
      fieldName = queryUnknownField(
          underlying, std::integral_constant<std::int64_t, 0>{},
          std::make_integer_sequence<std::int64_t, 128>{});
    } else if (static_cast<std::size_t>(underlying) >= rx::fieldCount<T>) {
      fieldName = queryUnknownField(
          underlying, std::integral_constant<std::int64_t, rx::fieldCount<T>>{},
          std::make_integer_sequence<std::int64_t, 128>{});
    } else {
      fieldName = getFieldName(underlying,
                               std::make_index_sequence<rx::fieldCount<T>>());
    }

    if (fieldName[0] >= '0' && fieldName[0] <= '9') {
      std::format_to(ctx.out(), "({}){}", rx::getNameOf<T>(), fieldName);
    } else {
      std::format_to(ctx.out(), "{}::{}", rx::getNameOf<T>(), fieldName);
    }

    return ctx.out();
  }
};

template <typename T>
  requires requires(T value) { std::begin(value) != std::end(value); }
struct std::formatter<T> {
  constexpr std::format_parse_context::iterator
  parse(std::format_parse_context &ctx) {
    return ctx.begin();
  }

  std::format_context::iterator format(T &s, std::format_context &ctx) const {
    std::format_to(ctx.out(), "[");

    for (bool first = true; auto &elem : s) {
      if (first) {
        first = false;
      } else {
        std::format_to(ctx.out(), ", ");
      }

      std::format_to(ctx.out(), "{}", elem);
    }

    std::format_to(ctx.out(), "]");
    return ctx.out();
  }
};

template <typename T>
  requires(!std::is_same_v<std::remove_cv_t<T>, char> &&
           !std::is_same_v<std::remove_cv_t<T>, wchar_t> &&
           !std::is_same_v<std::remove_cv_t<T>, char8_t> &&
           !std::is_same_v<std::remove_cv_t<T>, char16_t> &&
           !std::is_same_v<std::remove_cv_t<T>, char32_t> &&
           std::is_default_constructible_v<std::formatter<T>>)
struct std::formatter<T *> {
  constexpr std::format_parse_context::iterator
  parse(std::format_parse_context &ctx) {
    return ctx.begin();
  }

  std::format_context::iterator format(T *ptr, std::format_context &ctx) const {
    auto name = rx::detail::getVariableStorage().getVariableName(ptr);
    if (!name.empty()) {
      std::format_to(ctx.out(), "*{} = ", name);
    } else {
      std::format_to(ctx.out(), "*");
    }

    if (ptr == nullptr) {
      std::format_to(ctx.out(), "nullptr");
    } else {
      std::format_to(ctx.out(), "{}:{}", static_cast<void *>(ptr), *ptr);
    }
    return ctx.out();
  }
};

#define RX_CONCAT(a, b) RX_CONCAT_IMPL(a, b)
#define RX_CONCAT_IMPL(a, b) a##b

#define RX_REGISTER_VARIABLE(x)                                                \
  static auto RX_CONCAT(_RX_REGISTER_VARIABLE_, __LINE__) = ([] {              \
    ::rx::registerVariable<x>();                                               \
    return true;                                                               \
  }())

#define RX_REGISTER_FIELD(x)                                                   \
  static auto RX_CONCAT(_RX_REGISTER_FIELD_, __LINE__) = ([] {                 \
    ::rx::registerField<&x>();                                                 \
    return true;                                                               \
  }())
