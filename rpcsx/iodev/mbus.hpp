#pragma once

#include "io-device.hpp"
#include "iodev/MBusEvent.hpp"
#include "orbis/utils/SharedCV.hpp"
#include "orbis/utils/SharedMutex.hpp"


struct MBusDevice : IoDevice {
  orbis::shared_mutex mtx;
  orbis::shared_cv cv;
  orbis::kdeque<MBusEvent> events;
  orbis::Ref<orbis::EventEmitter> eventEmitter =
      orbis::knew<orbis::EventEmitter>();

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  void emitEvent(const MBusEvent &event);
};
