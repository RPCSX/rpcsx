#pragma once

#include "StaticString.hpp"
#include <algorithm>
#include <cstddef>
#include <string_view>

#ifdef _MSC_VER
#define RX_PRETTY_FUNCTION __FUNCSIG__
#elif defined(__GNUC__)
#define RX_PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define RX_PRETTY_FUNCTION ""
#endif

namespace rx {
namespace detail {
struct AnyStructFieldQuery {
  template <typename T> constexpr operator T();
};

// clang 21 crashes with concept
template <typename, typename, typename = void>
struct IsValidFieldCountImpl : std::false_type {};
template <typename StructT, std::size_t... I>
struct IsValidFieldCountImpl<
    StructT, std::index_sequence<I...>,
    std::void_t<decltype(StructT{(I, AnyStructFieldQuery{})...})>>
    : std::true_type {};
template <typename StructT, std::size_t I>
static constexpr bool IsValidFieldCount =
    IsValidFieldCountImpl<StructT,
                          decltype(std::make_index_sequence<I>{})>::value;

template <typename StructT, std::size_t Min = 1,
          std::size_t Max = std::min<std::size_t>(sizeof(StructT), 128),
          std::size_t Mid = (Max - Min) / 2>
struct CalcFieldCount;

template <typename StructT, std::size_t Min, std::size_t Max, std::size_t Mid>
  requires(Mid < Max && IsValidFieldCount<StructT, Mid>)
struct CalcFieldCount<StructT, Min, Max, Mid>
    : CalcFieldCount<StructT, Mid + 1, Max, (Mid + 1 + Max) / 2> {};

template <typename StructT, std::size_t Min, std::size_t Max, std::size_t Mid>
  requires(Mid < Max && !IsValidFieldCount<StructT, Mid>)
struct CalcFieldCount<StructT, Min, Max, Mid>
    : CalcFieldCount<StructT, Min, Mid, (Min + Mid) / 2> {};

template <typename StructT, std::size_t Min, std::size_t Max, std::size_t Mid>
  requires(Mid >= Max)
struct CalcFieldCount<StructT, Min, Max, Mid> {
  static constexpr auto count = Min == 0                          ? 0
                                : IsValidFieldCount<StructT, Min> ? Min
                                                                  : Min - 1;
};

template <typename StructT>
  requires std::is_class_v<StructT>
constexpr auto calcFieldCount() {
  return CalcFieldCount<StructT>::count;
}

consteval std::string_view unwrapName(std::string_view prefix,
                                      std::string_view pretty,
                                      bool dropNamespace) {
#ifdef _MSC_VER
  if (auto pos = pretty.find(prefix); pos != std::string_view::npos) {
    pretty.remove_prefix(pos + prefix.size());
  } else {
    pretty = {};
  }

  if (auto pos = pretty.rfind('>'); pos != std::string_view::npos) {
    pretty.remove_suffix(pretty.size() - pos);
  } else {
    pretty = {};
  }

  if (auto pos = pretty.rfind(')'); pos != std::string_view::npos) {
    pretty.remove_prefix(pos + 1);
  }

  if (auto pos = pretty.find(' '); pos != std::string_view::npos) {
    pretty.remove_prefix(pos + 1);
  }
#else
  if (auto pos = pretty.rfind('['); pos != std::string_view::npos) {
    pretty.remove_prefix(pos + 1);
  } else {
    pretty = {};
  }

  if (auto pos = pretty.rfind(prefix); pos != std::string_view::npos) {
    pretty.remove_prefix(pos + prefix.size());
  } else {
    pretty = {};
  }
  if (auto pos = pretty.rfind(')'); pos != std::string_view::npos) {
    pretty.remove_prefix(pos + 1);
  }
  if (pretty.ends_with(']')) {
    pretty.remove_suffix(1);
  } else {
    pretty = {};
  }
#endif

  if (dropNamespace) {
    if (auto pos = pretty.rfind(':'); pos != std::string_view::npos) {
      pretty.remove_prefix(pos + 1);
    }
  }

  return pretty;
}

template <typename> constexpr bool isField = false;

template <typename BaseT, typename TypeT>
constexpr bool isField<TypeT(BaseT::*)> = true;

} // namespace detail

template <auto &&V> constexpr auto getNameOf() {
  constexpr std::string_view prefix =
#ifdef _MSC_VER
      "getNameOf<";
#else
      "V = ";
#endif
  constexpr auto name = detail::unwrapName(prefix, RX_PRETTY_FUNCTION, true);
  static constexpr auto result = rx::StaticString<name.size() + 1>{name};
  return std::string_view{result};
}

template <auto V>
  requires(detail::isField<decltype(V)> ||
           std::is_enum_v<std::remove_cvref_t<decltype(V)>> ||
           std::is_pointer_v<std::remove_cvref_t<decltype(V)>>)
constexpr auto getNameOf() {
  constexpr std::string_view prefix =
#ifdef _MSC_VER
      "getNameOf<";
#else
      "V = ";
#endif

  constexpr auto name = detail::unwrapName(prefix, RX_PRETTY_FUNCTION, true);
  static constexpr auto result = rx::StaticString<name.size() + 1>{name};
  return std::string_view{result};
}

template <typename T> constexpr auto getNameOf() {
  constexpr std::string_view prefix =
#ifdef _MSC_VER
      "getNameOf<";
#else
      "T = ";
#endif
  constexpr auto name = detail::unwrapName(prefix, RX_PRETTY_FUNCTION, false);
  static constexpr auto result = rx::StaticString<name.size() + 1>{name};
  return std::string_view{result};
}

namespace detail {
template <typename EnumT, std::size_t N = 0>
  requires std::is_enum_v<EnumT>
constexpr auto calcFieldCount() {
  if constexpr (requires { EnumT::Count; }) {
    return static_cast<std::size_t>(EnumT::Count);
  } else if constexpr (requires { EnumT::_count; }) {
    return static_cast<std::size_t>(EnumT::_count);
  } else if constexpr (requires { EnumT::count; }) {
    return static_cast<std::size_t>(EnumT::count);
  } else if constexpr (!requires { getNameOf<EnumT(N)>()[0]; }) {
    return N;
  } else {
    constexpr auto c = getNameOf<EnumT(N)>()[0];
    if constexpr (!requires { getNameOf<EnumT(N)>()[0]; }) {
      return N;
    } else if constexpr (c >= '0' && c <= '9') {
      return N;
    } else {
      return calcFieldCount<EnumT, N + 1>();
    }
  }
}
} // namespace detail

template <typename StructT>
inline constexpr std::size_t fieldCount = detail::calcFieldCount<StructT>();
} // namespace rx
