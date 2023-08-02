#pragma once

#include <condition_variable>
#include <forward_list>
#include <functional>
#include <map>
#include <mutex>
#include <orbis/thread/Thread.hpp>
#include <sys/ucontext.h>
#include <thread>
#include <ucontext.h>
#include <utility>
#include <vector>

namespace rx {
class Scheduler {
  struct CpuState {
    std::mutex mtx;
    std::condition_variable cond;
    orbis::Thread *task = nullptr;
    ucontext_t cpuContext;
  };

  struct Task {
    orbis::Thread *thread;
    std::function<bool()> wakeupCondFn;
  };

  std::mutex taskMtx;
  std::condition_variable taskCond;
  std::mutex queueMtx;
  std::multimap<int, Task, std::greater<>> mQueue;
  std::forward_list<CpuState> mCpuStates;
  std::vector<std::thread> mCpus;
  std::thread mThread;
  std::atomic<bool> mExit{false};

public:
  Scheduler(std::size_t smpCount) {
    mCpus.resize(smpCount);

    for (std::size_t i = 0; i < smpCount; ++i) {
      auto state = &mCpuStates.emplace_front();
      mCpus[i] = std::thread{[=, this] { cpuEntry(i, state); }};
    }

    mThread = std::thread{[this] { schedulerEntry(); }};
  }

  ~Scheduler() {
    mExit = true;
    taskCond.notify_one();

    for (auto &cpuState : mCpuStates) {
      cpuState.cond.notify_one();
    }

    mThread.join();

    for (auto &cpu : mCpus) {
      cpu.join();
    }
  }

  void enqueue(orbis::Thread *thread) {
    std::lock_guard lockQueue(queueMtx);
    mQueue.emplace(thread->prio, Task{.thread = thread});
    taskCond.notify_one();
  }

  [[noreturn]] void
  releaseThisCpu(orbis::Thread *thread,
                 std::function<bool()> wakeupCondFn = nullptr) {
    auto cpuContext = static_cast<CpuState *>(thread->cpuContext);
    thread->cpuContext = nullptr;
    thread->cpuIndex = -1;
    mQueue.emplace(thread->prio, Task{.thread = thread,
                                      .wakeupCondFn = std::move(wakeupCondFn)});
    cpuContext->task = nullptr;
    ::setcontext(&cpuContext->cpuContext);
    __builtin_unreachable();
  }

private:
  [[noreturn]] void invoke(orbis::Thread *thread) {
    auto ctxt = reinterpret_cast<ucontext_t *>(thread->context);
    ::setcontext(ctxt);
    __builtin_unreachable();
  }

  orbis::Thread *fetchTask() {
    std::lock_guard lockQueue(queueMtx);
    decltype(mQueue)::iterator foundIt = mQueue.end();
    for (auto it = mQueue.begin(); it != mQueue.end(); ++it) {
      auto &[prio, task] = *it;
      if (task.wakeupCondFn != nullptr && !task.wakeupCondFn()) {
        continue;
      }

      if (foundIt == mQueue.end() || foundIt->first < task.thread->prio) {
        foundIt = it;
      }
    }

    if (foundIt != mQueue.end()) {
      auto result = foundIt->second.thread;
      mQueue.erase(foundIt);
      return result;
    }

    return nullptr;
  }

  void schedulerEntry() {
    while (!mExit.load(std::memory_order::relaxed)) {
      if (mQueue.empty()) {
        continue;
      }

      std::unique_lock lock(taskMtx);
      taskCond.wait(lock);

      for (auto &cpu : mCpuStates) {
        if (cpu.task == nullptr) {
          cpu.task = fetchTask();
        }
      }
    }
  }

  void cpuEntry(std::size_t cpuIndex, CpuState *state) {
    ::getcontext(&state->cpuContext);

    while (!mExit.load(std::memory_order::relaxed)) {
      auto task = std::exchange(state->task, nullptr);
      if (task == nullptr) {
        taskCond.notify_one();

        std::unique_lock lock(state->mtx);
        state->cond.wait(lock);
        task = std::exchange(state->task, nullptr);
      }

      if (task != nullptr) {
        task->cpuIndex = cpuIndex;
        task->cpuContext = state;
        invoke(task);
      }
    }
  }
};
} // namespace rx
