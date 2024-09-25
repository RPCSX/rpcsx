#pragma once

#include "Operand.hpp"
#include "PrintableWrapper.hpp"

namespace shader::ir {
template <typename ImplT> struct NodeWrapper;

using Node = NodeWrapper<NodeImpl>;

template <typename ImplT> struct NodeWrapper : PrintableWrapper<ImplT> {
  using PrintableWrapper<ImplT>::PrintableWrapper;
  using PrintableWrapper<ImplT>::operator=;

  auto getLocation() const { return this->impl->getLocation(); }
};
} // namespace ir
