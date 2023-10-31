#pragma once

#include "orbis-config.hpp"

namespace orbis {
struct AppInfo {
  uint32_t appId;
  uint32_t unk0;
  uint32_t unk1;
  uint32_t appType;
  char titleId[10];
  uint16_t unk2;
  uint32_t unk3;
  slong unk4;
  slong unk5;
  slong unk6;
  slong unk7;
  slong unk8;
};
static_assert(sizeof(AppInfo) == 72);
} // namespace orbis
