#pragma once

#include "Node.hpp"
#include <set>
#include <string>
#include <unordered_map>

namespace shader::ir {
class NameStorage {
  std::set<std::string> mNames;
  std::unordered_map<const NodeImpl *, const std::string *> mNodeToName;

public:
  void setUniqueNameOf(Node node, std::string name) {
    auto [nodeIt, nodeInserted] = mNodeToName.try_emplace(node.impl, nullptr);

    if (!nodeInserted && *nodeIt->second == name) {
      return;
    }

    auto [nameIt, nameInserted] = mNames.insert(name);

    if (!nameInserted) {
      std::size_t i = 1;

      while (true) {
        auto newName = name + "_" + std::to_string(i);
        auto [newNameIt, newNameInserted] = mNames.insert(std::move(newName));

        if (!newNameInserted) {
          ++i;
          continue;
        }

        nameIt = newNameIt;
        break;
      }
    }

    nodeIt->second = &*nameIt;
  }

  void setNameOf(Node node, std::string name) {
    auto [nodeIt, nodeInserted] = mNodeToName.try_emplace(node.impl, nullptr);

    if (!nodeInserted && *nodeIt->second == name) {
      return;
    }

    auto [nameIt, nameInserted] = mNames.insert(name);
    nodeIt->second = &*nameIt;
  }

  std::string_view tryGetNameOf(Node node) const {
    auto it = mNodeToName.find(node.impl);
    if (it == mNodeToName.end()) {
      return {};
    }
    return *it->second;
  }

  const std::string &getNameOf(Node node) {
    auto [it, inserted] = mNodeToName.emplace(node.impl, nullptr);

    if (inserted) {
      std::size_t i = mNames.size() + 1;

      while (true) {
        auto newName = std::to_string(i);
        auto [newNameIt, newNameInserted] = mNames.insert(std::move(newName));

        if (!newNameInserted) {
          ++i;
          continue;
        }

        it->second = &*newNameIt;
        break;
      }
    }

    return *it->second;
  }

  void clear() {
    mNames.clear();
    mNodeToName.clear();
  }
};
} // namespace shader::ir