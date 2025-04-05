#pragma once

#include "Node.hpp"
#include "RegionLike.hpp"

namespace shader::ir {
template <typename ImplT>
struct RegionWrapper : RegionLikeWrapper<ImplT, NodeWrapper> {
  using RegionLikeWrapper<ImplT, NodeWrapper>::RegionLikeWrapper;
  using RegionLikeWrapper<ImplT, NodeWrapper>::operator=;
};

struct RegionImpl;

struct Region : RegionWrapper<RegionImpl> {
  using RegionWrapper<RegionImpl>::RegionWrapper;
  using RegionWrapper<RegionImpl>::operator=;
};
} // namespace shader::ir
