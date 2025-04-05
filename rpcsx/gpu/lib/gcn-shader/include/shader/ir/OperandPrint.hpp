#pragma once

#include "NameStorage.hpp"
#include "Operand.hpp"
#include "ValueImpl.hpp" // IWYU pragma: keep

namespace shader::ir {
inline void Operand::print(std::ostream &os, NameStorage &ns) const {
  if (auto node = getAsValue()) {
    os << '%' << ns.getNameOf(node);
    return;
  }
  if (auto node = getAsString()) {
    os << '"' << *node << '"';
    return;
  }
  if (auto node = getAsInt32()) {
    os << *node << "i32";
    return;
  }
  if (auto node = getAsInt64()) {
    os << *node << "i64";
    return;
  }
  if (auto node = getAsFloat()) {
    os << *node << 'f';
    return;
  }
  if (auto node = getAsDouble()) {
    os << *node << 'd';
    return;
  }
  if (auto node = getAsBool()) {
    os << (*node ? "true" : "false");
    return;
  }
  if (isNull()) {
    os << "null";
    return;
  }
  os << "<invalid operand " << value.index() << ">";
}
} // namespace shader::ir
