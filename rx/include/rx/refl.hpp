#pragma once

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
  template <typename T> constexpr operator T &&();
};

template <typename StructT, std::size_t N = 1, std::size_t LastValidCount = 0>
  requires std::is_class_v<StructT>
constexpr auto calcFieldCount() {

  auto isValidFieldCount = []<std::size_t... I>(std::index_sequence<I...>) {
    return requires { StructT(((I, AnyStructFieldQuery{}))...); };
  };

  if constexpr (isValidFieldCount(std::make_index_sequence<N>())) {
    return calcFieldCount<StructT, N + 1, N>();
  } else if constexpr (sizeof(StructT) <= N || LastValidCount > 0) {
    return LastValidCount;
  } else {
    return calcFieldCount<StructT, N + 1, LastValidCount>();
  }
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

template <auto &&V> consteval auto getNameOf() {
  std::string_view prefix;
#ifdef _MSC_VER
  prefix = "getNameOf<";
#else
  prefix = "V = ";
#endif
  return detail::unwrapName(prefix, RX_PRETTY_FUNCTION, true);
}

template <auto V>
  requires(detail::isField<decltype(V)> ||
           std::is_enum_v<std::remove_cvref_t<decltype(V)>> ||
           std::is_pointer_v<std::remove_cvref_t<decltype(V)>>)
consteval auto getNameOf() {
  std::string_view prefix;
#ifdef _MSC_VER
  prefix = "getNameOf<";
#else
  prefix = "V = ";
#endif
  return detail::unwrapName(prefix, RX_PRETTY_FUNCTION, true);
}

template <typename T> consteval auto getNameOf() {
  std::string_view prefix;
#ifdef _MSC_VER
  prefix = "getNameOf<";
#else
  prefix = "T = ";
#endif
  return detail::unwrapName(prefix, RX_PRETTY_FUNCTION, false);
}

namespace detail {
template <typename EnumT, std::size_t N = 0>
  requires std::is_enum_v<EnumT>
constexpr auto calcFieldCount() {

  if constexpr (!requires { getNameOf<EnumT(N)>()[0]; }) {
    return N;
  } else {
    constexpr auto c = getNameOf<EnumT(N)>()[0];
    if constexpr (requires { EnumT::Count; }) {
      return EnumT::Count;
    } else if constexpr (requires { EnumT::_count; }) {
      return EnumT::_count;
    } else if constexpr (requires { EnumT::count; }) {
      return EnumT::count;
    } else if constexpr (!requires { getNameOf<EnumT(N)>()[0]; }) {
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
