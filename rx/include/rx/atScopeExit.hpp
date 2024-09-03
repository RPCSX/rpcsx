#pragma once

#include <utility>

namespace rx {
template <typename T> class atScopeExit {
  T _object;

public:
  atScopeExit(T &&object) : _object(std::forward<T>(object)) {}
  ~atScopeExit() { _object(); }
};
} // namespace rx
