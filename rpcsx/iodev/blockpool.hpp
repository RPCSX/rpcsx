#pragma once

#include "io-device.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Rc.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include <cstdint>

struct BlockPoolDevice : public IoDevice {
  orbis::shared_mutex mtx;
  std::uint64_t len = 0;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
  orbis::ErrorCode map(void **address, std::uint64_t len, std::int32_t prot,
                       std::int32_t flags, orbis::Thread *thread);
  orbis::ErrorCode unmap(void *address, std::uint64_t len,
                         orbis::Thread *thread);
};
