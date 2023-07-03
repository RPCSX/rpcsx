#pragma once

#include "ModuleSegment.hpp"

#include "orbis-config.hpp"

namespace orbis {
struct ModuleInfo {
  uint64_t size;
  char name[256];
  ModuleSegment segments[4];
  uint32_t segmentCount;
  uint8_t fingerprint[20];
};
} // namespace orbis
