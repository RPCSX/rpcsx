#pragma once

#include "Location.hpp"
#include "NodeImpl.hpp"
#include "Operand.hpp"

#include <forward_list>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

namespace shader::ir {
struct UniqPtrCompare {
  static bool operator()(const auto &lhs, const auto &rhs)
    requires requires { *lhs <=> *rhs; }
  {
    return (*lhs <=> *rhs) == std::strong_ordering::less;
  }
};

class Context {
  std::forward_list<std::unique_ptr<NodeImpl>> mNodes;
  std::set<std::unique_ptr<LocationImpl>, UniqPtrCompare> mLocations;
  std::unique_ptr<UnknownLocationImpl> mUnknownLocation;

public:
  Context() = default;
  Context(const Context &) = delete;
  Context(Context &&) = default;
  Context &operator=(Context &&) = default;

  template <typename T, typename... ArgsT>
    requires requires {
      typename T::underlying_type;
      requires std::is_constructible_v<typename T::underlying_type, ArgsT...>;
      requires std::is_base_of_v<NodeImpl, typename T::underlying_type>;
    }
  T create(ArgsT &&...args) {
    auto result = new typename T::underlying_type(std::forward<ArgsT>(args)...);
    mNodes.emplace_front(std::unique_ptr<NodeImpl>{result});
    return T(result);
  }

  template <typename T, typename... ArgsT>
    requires requires {
      typename T::underlying_type;
      requires std::is_constructible_v<typename T::underlying_type, ArgsT...>;
      requires std::is_base_of_v<LocationImpl, typename T::underlying_type>;
    }
  T getLocation(ArgsT &&...args) {
    auto result = std::make_unique<typename T::underlying_type>(
        std::forward<ArgsT>(args)...);
    auto ptr = mLocations.insert(std::move(result)).first->get();
    return T(static_cast<typename T::underlying_type *>(ptr));
  }

  PathLocation getPathLocation(std::string path) {
    return getLocation<PathLocation>(std::move(path));
  }
  TextFileLocation getTextFileLocation(PathLocation location,
                                       std::uint64_t line,
                                       std::uint64_t column = 0) {
    return getLocation<TextFileLocation>(location, line, column);
  }
  TextFileLocation getTextFileLocation(std::string path, std::uint64_t line,
                                       std::uint64_t column = 0) {
    return getLocation<TextFileLocation>(getPathLocation(path), line, column);
  }
  OffsetLocation getOffsetLocation(Location baseLocation,
                                   std::uint64_t offset) {
    return getLocation<OffsetLocation>(baseLocation, offset);
  }
  MemoryLocation getMemoryLocation(std::uint64_t address, std::uint64_t size) {
    return getLocation<MemoryLocation>(address, size);
  }
  UnknownLocation getUnknownLocation() {
    if (mUnknownLocation == nullptr) {
      mUnknownLocation = std::make_unique<UnknownLocationImpl>();
    }
    return mUnknownLocation.get();
  }
};
} // namespace shader::ir
