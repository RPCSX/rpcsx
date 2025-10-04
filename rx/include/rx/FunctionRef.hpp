#pragma once

#include <compare>
#include <type_traits>
#include <utility>

namespace rx {
template <typename> class FunctionRef;
template <typename RT, typename... ArgsT> class FunctionRef<RT(ArgsT...)> {
  void *context = nullptr;
  RT (*invoke)(void *, ArgsT...) = nullptr;

public:
  constexpr FunctionRef() = default;

  template <typename T>
    requires(!std::is_same_v<std::remove_cvref_t<T>, FunctionRef>)
  constexpr FunctionRef(T &&object)
    requires requires(ArgsT... args) { RT(object(args...)); }
      : context(
            const_cast<std::remove_const_t<std::remove_cvref_t<T>> *>(&object)),
        invoke(+[](void *context, ArgsT... args) -> RT {
          return (*reinterpret_cast<T *>(context))(std::move(args)...);
        }) {}

  template <typename... InvokeArgsT>
  constexpr RT operator()(InvokeArgsT &&...args) const
    requires requires(void *context) {
      invoke(context, std::forward<InvokeArgsT>(args)...);
    }
  {
    return invoke(context, std::forward<InvokeArgsT>(args)...);
  }

  constexpr explicit operator bool() const { return invoke != nullptr; }
  constexpr bool operator==(std::nullptr_t) const { return invoke == nullptr; }
  constexpr auto operator<=>(const FunctionRef &) const = default;
};
} // namespace rx
