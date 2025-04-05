#pragma once

#include "Kind.hpp"
#include "Node.hpp"

namespace shader::ir {
enum class InstructionId : std::uint32_t {};

constexpr InstructionId getInstructionId(ir::Kind kind, unsigned op) {
  return static_cast<InstructionId>(static_cast<std::uint32_t>(kind) |
                                    static_cast<std::uint32_t>(op) << 5);
}

constexpr ir::Kind getInstructionKind(InstructionId id) {
  return static_cast<ir::Kind>(static_cast<std::uint32_t>(id) & 0x1f);
}
constexpr unsigned getInstructionOp(InstructionId id) {
  return static_cast<unsigned>(static_cast<std::uint32_t>(id) >> 5);
}

struct Region;
struct InstructionImpl;
struct Instruction;

template <typename ImplT> struct InstructionWrapper : NodeWrapper<ImplT> {
  using NodeWrapper<ImplT>::NodeWrapper;
  using NodeWrapper<ImplT>::operator=;

  Kind getKind() const { return this->impl->kind; }
  unsigned getOp() const { return this->impl->op; }
  InstructionId getInstId() const {
    return getInstructionId(getKind(), getOp());
  }

  auto getParent() const { return this->impl->parent; };
  bool hasParent() const { return this->impl->parent != nullptr; }
  auto getNext() const { return Instruction(this->impl->next); }
  auto getPrev() const { return Instruction(this->impl->prev); }

  void addOperand(Operand operand) const { this->impl->addOperand(operand); }

  decltype(auto) replaceOperand(int index, Operand operand) const {
    return this->impl->replaceOperand(index, operand);
  }
  decltype(auto) eraseOperand(int index, int count = 1) const {
    return this->impl->eraseOperand(index, count);
  }
  void insertAfter(Node point, Node node) const {
    this->impl->insertAfter(point, node);
  }
  void erase() const { this->impl->erase(); }
  void remove() const { this->impl->remove(); }

  template <typename T = Node> auto children() const {
    return this->impl->template children<T>();
  }
  decltype(auto) getOperand(std::size_t i) const {
    return this->impl->getOperand(i);
  }
  decltype(auto) getOperands() const { return this->impl->getOperands(); }
  std::size_t getOperandCount() const { return getOperands().size(); }

  template <typename T>
    requires std::is_enum_v<T>
  void addOperand(T enumValue) {
    addOperand(std::to_underlying(enumValue));
  }
};

struct Instruction : InstructionWrapper<InstructionImpl> {
  using InstructionWrapper<InstructionImpl>::InstructionWrapper;
  using InstructionWrapper<InstructionImpl>::operator=;
};
} // namespace shader::ir
