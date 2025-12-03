#include "transform.hpp"
#include "transform/transformations.hpp"
#include "transform/wrap.hpp"
#include "SpvConverter.hpp"
#include "dialect.hpp"
#include <iostream>
#include <rx/die.hpp>

using namespace shader;
using namespace shader::transform;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

void shader::structurizeCfg(spv::Context &context, ir::RegionLike region) {
  // std::cerr << "before transforms: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";

  transformToCanonicalRegion(context, region);
  transformToCf(context, region);

  wrapLoopConstructs(context, region);
  wrapSelectionConstructs(context, region);

  // std::cerr << "structured: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";
  transformToFlat(context, region);

  // std::cerr << "flat: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";
}
