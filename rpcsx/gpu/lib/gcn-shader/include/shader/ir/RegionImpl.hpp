#pragma once
#include "NameStorage.hpp"
#include "NodeImpl.hpp"
#include "Region.hpp"
#include "RegionLikeImpl.hpp"
#include <ostream>

namespace shader::ir {
struct RegionImpl : NodeImpl, RegionLikeImpl {
  RegionImpl(Location loc) { setLocation(loc); }

  void print(std::ostream &os, NameStorage &ns) const override;
  Node clone(Context &context, CloneMap &map) const override;
};
} // namespace ir
