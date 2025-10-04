#pragma once

#include "io-device.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "rx/MemoryTable.hpp"
#include "rx/Rc.hpp"
#include "rx/SharedMutex.hpp"
#include <cstdint>

struct BlockPoolDevice : public IoDevice {
  rx::shared_mutex mtx;
  rx::MemoryAreaTable<> pool;

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
  orbis::ErrorCode map(void **address, std::uint64_t len, std::int32_t prot,
                       std::int32_t flags, orbis::Thread *thread);
  orbis::ErrorCode unmap(void *address, std::uint64_t len,
                         orbis::Thread *thread);
};
