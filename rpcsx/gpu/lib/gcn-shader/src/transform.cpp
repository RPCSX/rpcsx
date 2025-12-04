#include "transform.hpp"
#include "transform/transformations.hpp"
#include "transform/wrap.hpp"
#include <iostream>

using namespace shader;

void shader::structurizeCfg(spv::Context &context, ir::RegionLike region) {
  // std::cerr << "before transforms: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";

  transform::toCanonicalRegion(context, region);
  transform::toCf(context, region);

  transform::wrapLoopConstructs(context, region);
  transform::wrapSelectionConstructs(context, region);
  transform::canonicalizeSwitchSelectionConstructs(context, region);

  // std::cerr << "structured: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";

  transform::toFlat(context, region);

  // std::cerr << "flat: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";
}
