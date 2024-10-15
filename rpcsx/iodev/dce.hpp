#pragma once

#include "io-device.hpp"
#include "orbis-config.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/utils/Rc.hpp"
#include "orbis/utils/SharedMutex.hpp"

static constexpr auto kVmIdCount = 6;

struct DceDevice : IoDevice {
  orbis::shared_mutex mtx;
  std::uint32_t freeVmIds = (1 << (kVmIdCount + 1)) - 1;
  orbis::uint64_t dmemOffset = ~static_cast<std::uint64_t>(0);

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  int allocateVmId();
  void deallocateVmId(int vmId);
  void initializeProcess(orbis::Process *process);
};
