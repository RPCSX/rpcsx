#pragma once

#include "orbis-config.hpp"

namespace orbis {
static constexpr auto NCPUBITS = sizeof(slong) * 8;
static constexpr auto NCPUWORDS = 128 / NCPUBITS;

struct cpuset {
  slong bits[NCPUWORDS];
};
} // namespace orbis
