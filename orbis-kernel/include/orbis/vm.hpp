#pragma once

#include "orbis-config.hpp"

namespace orbis {
struct MemoryProtection {
  uint64_t startAddress;
  uint64_t endAddress;
  int32_t prot;
};

static_assert(sizeof(MemoryProtection) == 24);
} // namespace orbis
