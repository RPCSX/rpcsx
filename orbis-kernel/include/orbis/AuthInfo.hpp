#pragma once

#include "orbis-config.hpp"

namespace orbis {
struct AuthInfo {
  uint64_t unk0;
  uint64_t caps[4];
  uint64_t attrs[4];
  uint64_t ucred[8];

  bool hasUseHp3dPipeCapability() const {
    return ucred[2] == 0x3800000000000009;
  }
  bool hasMmapSelfCapability() const { return ((ucred[4] >> 0x3a) & 1) != 1; }
  bool hasSystemCapability() const { return ((ucred[3] >> 0x3e) & 1) != 0; }
  bool hasSceProgramAttribute() const { return ((ucred[3] >> 0x1f) & 1) != 0; }
};

static_assert(sizeof(AuthInfo) == 136);
} // namespace orbis
