#pragma once

#include "orbis-config.hpp"

namespace orbis {
struct AuthInfo {
  uint64_t unk0;
  uint64_t caps[4];
  uint64_t attrs[4];
  uint64_t unk[8];
};

static_assert(sizeof(AuthInfo) == 136);
} // namespace orbis
