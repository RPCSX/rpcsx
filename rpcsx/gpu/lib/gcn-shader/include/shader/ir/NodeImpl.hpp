#pragma once

#include "PrintOptions.hpp"
#include "Location.hpp"
#include "Node.hpp"
#include "Operand.hpp"
#include <cassert>
#include <map>

namespace shader::ir {
struct NodeImpl;
struct CloneMap;
class NameStorage;
class Context;

// namespace debug {
// [[gnu::used, gnu::noinline]] void dump(Node object);
// [[gnu::used, gnu::noinline]] void dump(NodeImpl *object);
// } // namespace debug

struct CloneMap {
  virtual ~CloneMap() = default;

  std::map<Node, Node> overrides;
  void setOverride(Node from, Node to) { overrides[from] = to; }
  Node getOverride(Node from) {
    if (auto it = overrides.find(from); it != overrides.end()) {
      return it->second;
    }
    return {};
  }
  virtual Node getOrClone(Context &context, Node node, bool isOperand) {
    // if (auto it = overrides.find(node); it != overrides.end()) {
    //   return it->second;
    // }

    // return getOrCloneImpl(context, node, isOperand);

    if (node == nullptr) {
      return node;
    }

    auto [it, inserted] = overrides.insert({node, nullptr});

    if (inserted) {
      it->second = getOrCloneImpl(context, node, isOperand);
      overrides[it->second] = it->second;
    }

    return it->second;
  }

  virtual Node getOrCloneImpl(Context &context, Node node, bool isOperand);
};

struct NodeImpl {
  Location location;
  virtual ~NodeImpl() = default;

  void setLocation(Location newLocation) { location = newLocation; }
  Location getLocation() const { return location; }

  virtual void print(std::ostream &os, NameStorage &ns, const PrintOptions &opts) const = 0;
  virtual Node clone(Context &context, CloneMap &map) const = 0;
};
} // namespace shader::ir
