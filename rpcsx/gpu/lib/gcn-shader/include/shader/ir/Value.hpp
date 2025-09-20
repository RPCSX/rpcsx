#pragma once

#include "Instruction.hpp"
#include "Operand.hpp"
#include "rx/FunctionRef.hpp"

namespace shader::ir {
struct Value;
struct ValueUse;

template <typename T> struct ValueWrapper : InstructionWrapper<T> {
  using InstructionWrapper<T>::InstructionWrapper;
  using InstructionWrapper<T>::operator=;

  decltype(auto) getUserList() const { return this->impl->getUserList(); }
  auto &getUseList() const { return this->impl->uses; }
  void replaceAllUsesWith(Value other) const;
  void replaceUsesIf(Value other, rx::FunctionRef<bool(ValueUse)> cb);

  bool isUnused() const { return this->impl->uses.empty(); }
};

struct ValueImpl;
struct Value : ValueWrapper<ValueImpl> {
  using ValueWrapper::ValueWrapper;
  using ValueWrapper::operator=;
};

struct ValueUse {
  Instruction user;
  Value node;
  int operandIndex;
  auto operator<=>(const ValueUse &) const = default;
};

template <typename T>
void ValueWrapper<T>::replaceAllUsesWith(Value other) const {
  this->impl->replaceAllUsesWith(other);
}

template <typename T>
void ValueWrapper<T>::replaceUsesIf(Value other,
                                    rx::FunctionRef<bool(ValueUse)> cb) {
  this->impl->replaceUsesIf(other, cb);
}
} // namespace shader::ir
