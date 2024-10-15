#pragma once

#include "orbis-config.hpp"

struct MBusEvent {
  orbis::uint32_t system;
  orbis::uint32_t eventId;
  orbis::uint64_t deviceId;
  orbis::uint32_t unk1; // device type?
  orbis::uint32_t subsystem;
  orbis::uint64_t unk2;
  orbis::uint64_t unk3;
  orbis::uint64_t unk4;
};

static_assert(sizeof(MBusEvent) == 0x30);
