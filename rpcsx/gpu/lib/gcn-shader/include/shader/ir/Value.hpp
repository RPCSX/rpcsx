#pragma once

#include "Instruction.hpp"
#include "Operand.hpp"

namespace shader::ir {
struct Value;
template <typename T> struct ValueWrapper : InstructionWrapper<T> {
  using InstructionWrapper<T>::InstructionWrapper;
  using InstructionWrapper<T>::operator=;

  decltype(auto) getUserList() const { return this->impl->getUserList(); }
  auto &getUseList() const { return this->impl->uses; }
  void replaceAllUsesWith(Value other) const;

  bool isUnused() const { return this->impl->uses.empty(); }
};

struct ValueImpl;
struct Value : ValueWrapper<ValueImpl> {
  using ValueWrapper::ValueWrapper;
  using ValueWrapper::operator=;
};

template <typename T>
void ValueWrapper<T>::replaceAllUsesWith(Value other) const {
  this->impl->replaceAllUsesWith(other);
}

struct ValueUse {
  Instruction user;
  Value node;
  int operandIndex;
  auto operator<=>(const ValueUse &) const = default;
};
} // namespace shader::ir
