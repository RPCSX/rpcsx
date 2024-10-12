#pragma once

#include <cassert>
#include <functional>
#include <type_traits>

namespace shader::ir {
template <typename ImplT> struct PointerWrapper {
  using underlying_type = ImplT;
  ImplT *impl = nullptr;
  PointerWrapper() = default;
  PointerWrapper(ImplT *impl) : impl(impl) {}

  template <typename OtherT>
    requires std::is_base_of_v<ImplT, OtherT>
  PointerWrapper(PointerWrapper<OtherT> node) : impl(node.impl) {}

  explicit operator bool() const { return impl != nullptr; }
  bool operator==(std::nullptr_t) const { return impl == nullptr; }
  bool operator==(ImplT *other) const { return impl == other; }

  template <typename Self> Self &operator=(this Self &self, ImplT *other) {
    self.impl = other;
    return self;
  }

  template <typename Self, typename OtherT>
    requires std::is_base_of_v<ImplT, OtherT>
  Self &operator=(this Self &self, PointerWrapper<OtherT> other) {
    self.impl = other.get();
    return self;
  }

  // ImplT *operator->() const { return impl; }

  ImplT *get() const { return impl; }

  auto operator<=>(const PointerWrapper &) const = default;
  bool operator==(const PointerWrapper &) const = default;

  template <typename T>
  T cast() const
    requires requires { static_cast<typename T::underlying_type *>(impl); }
  {
    return T(dynamic_cast<typename T::underlying_type *>(impl));
  }

  template <typename T>
  T staticCast() const
    requires requires { static_cast<typename T::underlying_type *>(impl); }
  {
    assert(impl == nullptr || cast<T>() != nullptr);
    return T(static_cast<typename T::underlying_type *>(impl));
  }

  template <typename T> bool isa() const {
    if (impl == nullptr) {
      return false;
    }

    if constexpr (std::is_same_v<std::remove_cvref_t<T>,
                                 std::remove_cvref_t<ImplT>>) {
      return true;
    } else if constexpr (!requires { cast<T>() != nullptr; }) {
      return false;
    } else {
      return cast<T>() != nullptr;
    }
  }

  template <typename... T>
    requires(sizeof...(T) > 1)
  bool isa() const {
    return (isa<T>() || ...);
  }
};
} // namespace shader::ir

namespace std {
template <typename T>
  requires std::is_base_of_v<
      shader::ir::PointerWrapper<typename T::underlying_type>, T>
struct hash<T> {
  constexpr std::size_t operator()(const T &pointer) const noexcept {
    return hash<typename T::underlying_type *>{}(pointer.impl);
  }
};
} // namespace std
