#pragma once

#include "PointerWrapper.hpp"
#include <ostream>

namespace shader::ir {
class NameStorage;
template <typename T> struct PrintableWrapper : PointerWrapper<T> {
  using PointerWrapper<T>::PointerWrapper;
  using PointerWrapper<T>::operator=;

  void print(std::ostream &os, NameStorage &ns) const {
    if constexpr (requires { this->impl->print(os, ns); }) {
      this->impl->print(os, ns);
    } else {
      this->impl->print(os);
    }
  }

  void print(std::ostream &os) const
    requires requires { this->impl->print(os); }
  {
    this->impl->print(os);
  }
};
} // namespace shader::ir
