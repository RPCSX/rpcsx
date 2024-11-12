#pragma once

#include "refl.hpp"
#include <compare>
#include <cstddef>
#include <functional>

namespace rx {
namespace detail {
template <typename T> constexpr std::string_view mRawTypeId = getNameOf<T>();

template <typename T>
[[nodiscard]] constexpr const std::string_view *getTypeIdImpl() {
  return &mRawTypeId<T>;
}
} // namespace detail

class TypeId {
  const std::string_view *mId = detail::getTypeIdImpl<void>();

public:
  [[nodiscard]] constexpr const void *getOpaque() const { return mId; }

  [[nodiscard]] constexpr static TypeId createFromOpaque(const void *id) {
    TypeId result;
    result.mId = static_cast<const std::string_view *>(id);
    return result;
  }

  template <typename T> [[nodiscard]] constexpr static TypeId get() {
    TypeId result;
    result.mId = detail::getTypeIdImpl<T>();
    return result;
  }

  [[nodiscard]] constexpr std::string_view getName() const { return *mId; }

  constexpr auto operator<=>(const TypeId &other) const = default;
};

template <typename... Types>
constexpr std::array<TypeId, sizeof...(Types)> getTypeIds() {
  return std::array<TypeId, sizeof...(Types)>{TypeId::get<Types>()...};
}
} // namespace rx

namespace std {
template <> struct hash<rx::TypeId> {
  constexpr std::size_t operator()(const rx::TypeId &id) const noexcept {
    return std::hash<const void *>{}(id.getOpaque());
  }
};
} // namespace std
