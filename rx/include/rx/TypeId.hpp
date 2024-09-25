#pragma once

#include <compare>
#include <cstddef>
#include <functional>

namespace rx {
namespace detail {
template <typename> char mRawTypeId = 0;
template <typename T> constexpr const void *getTypeIdImpl() {
  return &mRawTypeId<T>;
}
} // namespace detail

class TypeId {
  const void *mId = detail::getTypeIdImpl<void>();

public:
  constexpr const void *getOpaque() const { return mId; }

  constexpr static TypeId createFromOpaque(const void *id) {
    TypeId result;
    result.mId = id;
    return result;
  }

  template <typename T> constexpr static TypeId get() {
    return createFromOpaque(detail::getTypeIdImpl<T>());
  }

  constexpr auto operator<=>(const TypeId &other) const = default;
};
} // namespace rx

namespace std {
template <> struct hash<rx::TypeId> {
  constexpr std::size_t operator()(const rx::TypeId &id) const noexcept {
    return std::hash<const void *>{}(id.getOpaque());
  }
};
} // namespace std
