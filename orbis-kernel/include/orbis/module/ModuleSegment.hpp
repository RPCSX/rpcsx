#pragma once

#include "orbis-config.hpp"

namespace orbis {
struct ModuleSegment {
  ptr<void> addr;
  uint32_t size;
  uint32_t prot;
};
} // namespace orbis
