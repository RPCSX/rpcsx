#pragma once
#include "../dialect/builtin.hpp"
#include "../dialect/memssa.hpp"
#include "Block.hpp"
#include "Context.hpp"
#include "InstructionImpl.hpp"
#include "LoopConstruct.hpp"
#include "NodeImpl.hpp"
#include "RegionImpl.hpp"
#include "SelectionConstruct.hpp"
#include "ValueImpl.hpp"

namespace shader::ir {
inline void InstructionImpl::addOperand(Operand operand) {
  if (operand != nullptr) {
    if (auto value = operand.getAsValue()) {
      value.get()->addUse(this, operands.size());
    }
  }

  operands.addOperand(std::move(operand));
}

inline Operand InstructionImpl::replaceOperand(int index, Operand operand) {
  if (operands.size() <= unsigned(index)) {
    std::abort();
  }

  if (!operands[index].isNull()) {
    if (auto value = operands[index].getAsValue()) {
      value.get()->removeUse(this, index);
    }
  }

  if (auto value = operand.getAsValue()) {
    value.get()->addUse(this, index);
  }

  return std::exchange(operands[index], std::move(operand));
}

inline Operand InstructionImpl::eraseOperand(int index, int count) {
  if (index + count == operands.size()) {
    auto result = replaceOperand(index, nullptr);

    for (std::size_t i = index + 1; i < operands.size(); ++i) {
      replaceOperand(i, nullptr);
    }

    operands.resize(operands.size() - count);
    return result;
  }

  auto result = replaceOperand(index, replaceOperand(index + 1, nullptr));

  for (std::size_t i = index + 1; i < operands.size() - count; ++i) {
    replaceOperand(i, replaceOperand(i + count, nullptr));
  }

  operands.resize(operands.size() - count);
  return result;
}

inline void InstructionImpl::remove() {
  if (auto value = Instruction(this).cast<Value>()) {
    if (!value.isUnused()) {
      std::abort();
    }
  }

  for (int index = 0; auto &operand : operands) {
    if (auto value = operand.getAsValue()) {
      value.get()->removeUse(this, index);
    }
    index++;
  }

  operands.clear();

  if (parent != nullptr) {
    erase();
  }
}

inline void InstructionImpl::erase() {
  assert(parent != nullptr);

  if (prev != nullptr) {
    prev.get()->next = next;
  } else {
    parent.get()->first = next;
  }
  if (next != nullptr) {
    next.get()->prev = prev;
  } else {
    parent.get()->last = prev;
  }

  prev = nullptr;
  next = nullptr;
  parent = nullptr;
}

template <typename ImplT, template <typename> typename BaseWrapper>
void RegionLikeWrapper<ImplT, BaseWrapper>::appendRegion(RegionLike other) {
  for (auto child = other.getFirst(); child != nullptr;) {
    auto node = child;
    child = child.getNext();
    node.erase();
    this->addChild(node);
  }
}

inline void RegionLikeImpl::insertAfter(Instruction point, Instruction node) {
  assert(point == nullptr || point.getParent() == this);
  assert(node.getParent() == nullptr);
  assert(node.getPrev() == nullptr);
  assert(node.getNext() == nullptr);

  if (point == nullptr) {
    prependChild(node);
    return;
  }

  assert(first != nullptr);
  assert(last != nullptr);

  node.get()->parent = this;
  node.get()->prev = point.get();

  if (auto pointNext = point.getNext()) {
    pointNext.get()->prev = node.get();
    node.get()->next = pointNext.get();
  } else {
    assert(last == point);
    last = node.get();
  }

  point.get()->next = node.get();
}

inline void RegionLikeImpl::prependChild(Instruction node) {
  assert(node.getParent() == nullptr);
  assert(node.getPrev() == nullptr);
  assert(node.getNext() == nullptr);

#ifndef NDEBUG
  if (auto thisInst = dynamic_cast<InstructionImpl *>(this)) {
    assert(node != thisInst);
  }
#endif

  node.get()->parent = this;
  if (last == nullptr) {
    last = node;
  } else {
    first.get()->prev = node;
    node.get()->next = first;
  }
  first = node;
}

inline void RegionLikeImpl::addChild(Instruction node) {
  assert(node.getParent() == nullptr);
  assert(node.getPrev() == nullptr);
  assert(node.getNext() == nullptr);

#ifndef NDEBUG
  if (auto thisInst = dynamic_cast<InstructionImpl *>(this)) {
    assert(node != thisInst);
  }
#endif
  
  node.get()->parent = this;
  if (first == nullptr) {
    first = node;
  } else {
    last.get()->next = node;
    node.get()->prev = last;
  }
  last = node;
}

inline void RegionLikeImpl::printRegion(std::ostream &os, NameStorage &ns,
                                        const PrintOptions &opts) const {
  if (auto node = dynamic_cast<const NodeImpl *>(this)) {
    node->print(os, ns, opts);
  } else {
    os << "<detached region>";
  }
}

inline auto RegionLikeImpl::getParent() const {
  if (auto inst = dynamic_cast<const InstructionImpl *>(this)) {
    return inst->parent;
  }

  return RegionLike();
}

inline void RegionImpl::print(std::ostream &os, NameStorage &ns,
                              const PrintOptions &opts) const {
  opts.printIdent(os);
  os << "{\n";
  for (auto childIdent = opts.nextLevel(); auto child : children()) {
    childIdent.printIdent(os);
    child.print(os, ns, childIdent);
    os << "\n";
  }
  opts.printIdent(os);
  os << "}";
}

inline Value Operand::getAsValue() const {
  if (auto node = std::get_if<ValueImpl *>(&value)) {
    return Value(const_cast<ValueImpl *>(*node));
  }

  return {};
}

template <typename T>
T clone(T object, Context &context, CloneMap &map, bool isOperand = false)
  requires requires {
    map.getOrClone(context, object, isOperand).template staticCast<T>();
  }
{
  return map.getOrClone(context, object, isOperand).template staticCast<T>();
}

template <typename T>
T clone(T object, Context &context)
  requires requires(CloneMap map) { clone(object, context, map); }
{
  CloneMap map;
  return clone(object, context, map);
}

template <typename T>
T clone(T location, Context &context)
  requires requires { Location(location).get()->clone(context); }
{
  if (location == nullptr) {
    return nullptr;
  }
  return Location(location).get()->clone(context).staticCast<T>();
}

namespace detail {
template <typename T, typename U, typename... ArgsT>
  requires(std::is_same_v<typename T::underlying_type, U>)
T cloneInstructionImpl(const U *object, Context &context, CloneMap &map,
                       ArgsT &&...args) {
  auto result = context.create<T>(clone(object->getLocation(), context),
                                  std::forward<ArgsT>(args)...);

  for (auto &&operand : object->getOperands()) {
    result.addOperand(operand.clone(context, map));
  }

  return result;
}

template <typename T, typename U, typename... ArgsT>
  requires(std::is_same_v<typename T::underlying_type, U>)
T cloneBlockImpl(const U *object, Context &context, CloneMap &map,
                 ArgsT &&...args) {
  auto result = context.create<T>(clone(object->getLocation(), context),
                                  std::forward<ArgsT>(args)...);

  for (auto &&operand : object->getOperands()) {
    result.addOperand(operand.clone(context, map));
  }

  for (auto &&child : object->children()) {
    result.addChild(ir::clone(child, context, map));
  }

  return result;
}
} // namespace detail

inline Node InstructionImpl::clone(Context &context, CloneMap &map) const {
  return detail::cloneInstructionImpl<Instruction>(this, context, map, kind,
                                                   op);
}

inline Node ValueImpl::clone(Context &context, CloneMap &map) const {
  return detail::cloneInstructionImpl<Value>(this, context, map, kind, op);
}

inline Node RegionImpl::clone(Context &context, CloneMap &map) const {
  auto result = context.create<Region>(ir::clone(getLocation(), context));
  for (auto &&child : children()) {
    result.addChild(ir::clone(child, context, map));
  }

  return result;
}

inline Node BlockImpl::clone(Context &context, CloneMap &map) const {
  return detail::cloneBlockImpl<Block>(this, context, map, kind, op);
}

inline Node ContinueConstructImpl::clone(Context &context,
                                         CloneMap &map) const {
  return detail::cloneBlockImpl<ContinueConstruct>(this, context, map, kind,
                                                   op);
}

inline Node LoopConstructImpl::clone(Context &context, CloneMap &map) const {
  return detail::cloneBlockImpl<LoopConstruct>(this, context, map, kind, op);
}

inline Node SelectionConstructImpl::clone(Context &context,
                                          CloneMap &map) const {
  return detail::cloneBlockImpl<SelectionConstruct>(this, context, map, kind,
                                                    op);
}

inline Operand Operand::clone(Context &context, CloneMap &map) const {
  if (auto value = getAsValue()) {
    return ir::clone(value, context, map, true);
  }

  return *this;
}

inline Node memssa::PhiImpl::clone(Context &context, CloneMap &map) const {
  auto self = Phi(const_cast<PhiImpl *>(this));
  auto result = context.create<Phi>(ir::clone(self.getLocation(), context),
                                    self.getKind(), self.getOp());

  for (auto &&operand : self.getOperands()) {
    result.addOperand(operand.clone(context, map));
  }

  return result;
}

inline Node memssa::VarImpl::clone(Context &context, CloneMap &map) const {
  auto self = Var(const_cast<VarImpl *>(this));
  auto result = context.create<Var>(ir::clone(self.getLocation(), context),
                                    self.getKind(), self.getOp());

  for (auto &&operand : self.getOperands()) {
    result.addOperand(operand.clone(context, map));
  }

  return result;
}

inline Node memssa::UseImpl::clone(Context &context, CloneMap &map) const {
  auto self = Use(const_cast<UseImpl *>(this));
  auto result = context.create<Use>(ir::clone(self.getLocation(), context),
                                    self.getKind(), self.getOp());

  for (auto &&operand : self.getOperands()) {
    result.addOperand(operand.clone(context, map));
  }

  return result;
}

inline Node memssa::DefImpl::clone(Context &context, CloneMap &map) const {
  auto self = Def(const_cast<DefImpl *>(this));
  auto result = context.create<Def>(ir::clone(self.getLocation(), context),
                                    self.getKind(), self.getOp());

  for (auto &&operand : self.getOperands()) {
    result.addOperand(operand.clone(context, map));
  }

  return result;
}

inline Node memssa::ScopeImpl::clone(Context &context, CloneMap &map) const {
  auto self = Scope(const_cast<ScopeImpl *>(this));
  auto result =
      context.create<Scope>(ir::clone(self.getLocation(), context), kind, op);

  for (auto &&operand : self.getOperands()) {
    result.addOperand(operand.clone(context, map));
  }

  for (auto child : self.children()) {
    result.addChild(ir::clone(child, context, map));
  }

  return result;
}

inline Location PathLocationImpl::clone(Context &context) const {
  return context.getPathLocation(data.path);
}
inline Location TextFileLocationImpl::clone(Context &context) const {
  return context.getTextFileLocation(data.file, data.line, data.column);
}
inline Location OffsetLocationImpl::clone(Context &context) const {
  return context.getOffsetLocation(baseLocation, offset);
}
inline Location MemoryLocationImpl::clone(Context &context) const {
  return context.getMemoryLocation(data.address, data.size);
}
inline Location UnknownLocationImpl::clone(Context &context) const {
  return context.getUnknownLocation();
}

inline Node CloneMap::getOrCloneImpl(Context &context, Node node, bool) {
  Node result = node.get()->clone(context, *this);
  overrides[node] = result;
  return result;
}
} // namespace shader::ir
