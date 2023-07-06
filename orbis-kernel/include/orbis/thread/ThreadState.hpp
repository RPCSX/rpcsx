#pragma once

#include <cstdint>

namespace orbis {
enum class ThreadState : std::uint32_t {
  INACTIVE,
  INHIBITED,
  CAN_RUN,
  RUNQ,
  RUNNING
};
} // namespace orbis
