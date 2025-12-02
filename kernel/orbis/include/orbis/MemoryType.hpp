#pragma once

#include <cstdint>

namespace orbis {
enum class MemoryType : std::uint32_t {
  Invalid = -1u,
  WbOnion = 0,   // write back, CPU bus
  WcGarlic = 3,  // combining, GPU bus
  WbGarlic = 10, // write back, GPU bus
};
}
