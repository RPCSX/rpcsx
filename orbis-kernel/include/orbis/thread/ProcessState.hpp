#pragma once

#include <cstdint>

namespace orbis {
enum class ProcessState : std::uint32_t {
  NEW,    // In creation
  NORMAL, // threads can be run
  ZOMBIE
};
} // namespace orbis
