#pragma once

#include "io-device.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Rc.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include <cstdint>
#include <unistd.h>

struct DmemDevice : public IoDevice {
  orbis::shared_mutex mtx;
  int index;
  std::uint64_t nextOffset;
  std::uint64_t memBeginAddress;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  orbis::ErrorCode mmap(void **address, std::uint64_t len,
                        std::int32_t memoryType, std::int32_t prot,
                        std::int32_t flags, std::int64_t directMemoryStart);
};
