#pragma once

#include "orbis-config.hpp"
#include "types.hpp"
#include "ThreadState.hpp"

#include <mutex>

namespace orbis {
struct Process;
struct Thread {
  std::mutex lock;
  Process *tproc = nullptr;
  uint64_t retval[2]{};
  void *context{};
  ptr<void> stackStart;
  ptr<void> stackEnd;
  uint64_t fsBase{};
  uint64_t gsBase{};
  char name[32];

  uint64_t sigMask[4] = {
    0x7fff'ffff,
    0
  };

  lwpid_t tid = -1;
  ThreadState state = ThreadState::INACTIVE;
};
} // namespace orbis
