#pragma once

#include "io-device.hpp"
#include "orbis/utils/SharedCV.hpp"
#include "orbis/utils/SharedMutex.hpp"

struct MBusAVEvent {
  orbis::uint32_t unk0;
  orbis::uint32_t unk1;
  orbis::uint64_t unk2;
  char unk3[0x20];
};

static_assert(sizeof(MBusAVEvent) == 0x30);

struct MBusAVDevice : IoDevice {
  orbis::shared_mutex mtx;
  orbis::shared_cv cv;
  orbis::kdeque<MBusAVEvent> events;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  void emitEvent(const MBusAVEvent &event);
};
