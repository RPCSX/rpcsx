#pragma once

#include "ThreadState.hpp"
#include "orbis-config.hpp"
#include "types.hpp"

#include "../KernelAllocator.hpp"
#include "../ucontext.hpp"
#include "../utils/SharedCV.hpp"
#include "../utils/SharedMutex.hpp"
#include <atomic>
#include <thread>

namespace orbis {
struct Process;

struct Thread {
  utils::shared_mutex mtx;
  Process *tproc = nullptr;
  uint64_t retval[2]{};
  void *context{};
  ptr<void> stackStart;
  ptr<void> stackEnd;
  uint64_t fsBase{};
  uint64_t gsBase{};
  char name[32]{};

  SigSet sigMask = {0x7fff'ffff, ~0u, ~0u, ~0u};
  utils::shared_mutex suspend_mtx;
  utils::shared_cv suspend_cv;
  kdeque<int> signalQueue;
  kvector<UContext> sigReturns;
  std::atomic<unsigned> suspended{0};

  std::int64_t hostTid = -1;
  lwpid_t tid = -1;
  ThreadState state = ThreadState::INACTIVE;
  std::thread handle;

  // Used to wake up thread in sleep queue
  utils::shared_cv sync_cv;
  uint64_t evfResultPattern;
  uint64_t evfIsCancelled;

  // Print backtrace
  void where();

  void suspend();
  void resume();
  void sendSignal(int signo);

  // FIXME: implement thread destruction
  void incRef() {}
  void decRef() {}
};

extern thread_local Thread *g_currentThread;
} // namespace orbis
