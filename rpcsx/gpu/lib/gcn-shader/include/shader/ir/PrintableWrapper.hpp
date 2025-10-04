#pragma once

#include "PointerWrapper.hpp"
#include "PrintOptions.hpp"
#include <ostream>

namespace shader::ir {
class NameStorage;
template <typename T> struct PrintableWrapper : PointerWrapper<T> {
  using PointerWrapper<T>::PointerWrapper;
  using PointerWrapper<T>::operator=;

  void print(std::ostream &os, NameStorage &ns,
             const PrintOptions &opts = {}) const {
    if constexpr (requires { this->impl->print(os, ns, opts); }) {
      this->impl->print(os, ns, opts);
    } else if constexpr (requires { this->impl->print(os, ns); }) {
      this->impl->print(os, ns);
    } else if constexpr (requires { this->impl->print(os, opts); }) {
      this->impl->print(os, opts);
    } else {
      this->impl->print(os);
    }
  }

  void print(std::ostream &os, const PrintOptions &opts = {}) const
    requires(
        requires { this->impl->print(os, opts); } ||
        requires { this->impl->print(os); })
  {
    if constexpr (requires { this->impl->print(os, opts); }) {
      this->impl->print(os, opts);
    } else {
      this->impl->print(os);
    }
  }
};
} // namespace shader::ir
