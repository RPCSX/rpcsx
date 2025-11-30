#pragma once

#include "orbis-config.hpp"
#include "orbis/IoDevice.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "rx/Rc.hpp"
#include "rx/SharedMutex.hpp"

static constexpr auto kVmIdCount = 6;

struct DceDevice : orbis::IoDevice {
  rx::shared_mutex mtx;
  rx::AddressRange dmemRange;
  std::uint32_t eopCount = 0;
  std::uint32_t freeVmIds = (1 << (kVmIdCount + 1)) - 1;

  DceDevice() { blockFlags = orbis::vmem::BlockFlags::DirectMemory; }
  ~DceDevice();

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
  orbis::ErrorCode map(rx::AddressRange range, std::int64_t offset,
                       rx::EnumBitSet<orbis::vmem::Protection> protection,
                       orbis::File *file, orbis::Process *process) override;
  int allocateVmId();
  void deallocateVmId(int vmId);
  void initializeProcess(orbis::Process *process);
};
