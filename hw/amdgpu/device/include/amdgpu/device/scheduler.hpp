#pragma once

#include "util/unreachable.hpp"
#include <atomic>
#include <bit>
#include <cassert>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <utility>
#include <vector>

namespace amdgpu::device {
inline void setThreadName(const char *name) {
  pthread_setname_np(pthread_self(), name);
}

template <typename T> class Ref {
  T *m_ref = nullptr;

public:
  Ref() = default;
  Ref(std::nullptr_t) {}

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref(OT *ref) : m_ref(ref) {
    if (m_ref != nullptr) {
      ref->incRef();
    }
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref(const Ref<OT> &other) : m_ref(other.get()) {
    if (m_ref != nullptr) {
      m_ref->incRef();
    }
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref(Ref<OT> &&other) : m_ref(other.release()) {}

  Ref(const Ref &other) : m_ref(other.get()) {
    if (m_ref != nullptr) {
      m_ref->incRef();
    }
  }
  Ref(Ref &&other) : m_ref(other.release()) {}

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref &operator=(Ref<OT> &&other) {
    other.swap(*this);
    return *this;
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref &operator=(OT *other) {
    *this = Ref(other);
    return *this;
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref &operator=(const Ref<OT> &other) {
    *this = Ref(other);
    return *this;
  }

  Ref &operator=(const Ref &other) {
    *this = Ref(other);
    return *this;
  }

  Ref &operator=(Ref &&other) {
    other.swap(*this);
    return *this;
  }

  ~Ref() {
    if (m_ref != nullptr) {
      m_ref->decRef();
    }
  }

  void swap(Ref<T> &other) { std::swap(m_ref, other.m_ref); }
  T *get() const { return m_ref; }
  T *release() { return std::exchange(m_ref, nullptr); }
  T *operator->() const { return m_ref; }
  explicit operator bool() const { return m_ref != nullptr; }
  bool operator==(std::nullptr_t) const { return m_ref == nullptr; }
  bool operator==(const Ref &other) const = default;
  bool operator==(const T *other) const { return m_ref == other; }
  auto operator<=>(const T *other) const { return m_ref <=> other; }
  auto operator<=>(const Ref &other) const = default;
};

template <typename T> Ref(T *) -> Ref<T>;
template <typename T> Ref(Ref<T>) -> Ref<T>;

enum class TaskState { Created, InProgress, Complete, Canceled };
enum class TaskResult { Complete, Canceled, Reschedule };

struct AsyncTaskCtl {
  std::atomic<unsigned> refs{0};
  std::atomic<TaskState> stateStorage{TaskState::Created};
  std::atomic<bool> cancelRequested{false};

  virtual ~AsyncTaskCtl() = default;

  void incRef() { refs.fetch_add(1, std::memory_order::relaxed); }
  void decRef() {
    if (refs.fetch_sub(1, std::memory_order::relaxed) == 1) {
      delete this;
    }
  }

  bool isCancelRequested() const {
    return cancelRequested.load(std::memory_order::relaxed) == true;
  }
  bool isCanceled() const { return getState() == TaskState::Canceled; }
  bool isComplete() const { return getState() == TaskState::Complete; }
  bool isInProgress() const { return getState() == TaskState::InProgress; }

  TaskState getState() const {
    return stateStorage.load(std::memory_order::relaxed);
  }

  void cancel() { cancelRequested.store(true, std::memory_order::relaxed); }

  void wait() {
    if (stateStorage.load(std::memory_order::relaxed) == TaskState::Created) {
      util::unreachable("attempt to wait task that wasn't scheduled\n");
    }
    stateStorage.wait(TaskState::InProgress, std::memory_order::relaxed);
  }
};

struct CpuTaskCtl : AsyncTaskCtl {
  virtual TaskResult invoke() = 0;
};

namespace detail {
template <typename T>
concept LambdaWithoutClosure = requires(T t) { +t; };
}

template <typename T> struct AsyncCpuTask;

template <typename T>
  requires requires(T t, const AsyncTaskCtl &ctl) {
    { t(ctl) } -> std::same_as<TaskResult>;
    requires detail::LambdaWithoutClosure<T>;
  }
struct AsyncCpuTask<T> : CpuTaskCtl {
  static constexpr TaskResult (*fn)(const AsyncTaskCtl &) = +std::declval<T>();

  AsyncCpuTask() = default;
  AsyncCpuTask(T &&) {}

  TaskResult invoke() override {
    auto &base = *static_cast<const AsyncTaskCtl *>(this);

    return fn(base);
  }
};

template <typename T>
  requires requires(T t, const AsyncTaskCtl &ctl) {
    { t(ctl) } -> std::same_as<TaskResult>;
    requires !detail::LambdaWithoutClosure<T>;
  }
struct AsyncCpuTask<T> : CpuTaskCtl {
  alignas(T) std::byte taskStorage[sizeof(T)];

  AsyncCpuTask(T &&t) { new (taskStorage) T(std::forward<T>(t)); }
  ~AsyncCpuTask() { std::bit_cast<T *>(&taskStorage)->~T(); }

  TaskResult invoke() override {
    auto &lambda = *std::bit_cast<T *>(&taskStorage);
    auto &base = *static_cast<const AsyncTaskCtl *>(this);
    return lambda(base);
  }
};

template <typename T>
  requires requires(T t, const AsyncTaskCtl &ctl) {
    { t(ctl) } -> std::same_as<TaskResult>;
  }
Ref<CpuTaskCtl> createCpuTask(T &&task) {
  return Ref<CpuTaskCtl>(new AsyncCpuTask<T>(std::forward<T>(task)));
}

template <typename T>
  requires requires(T t) {
    { t() } -> std::same_as<TaskResult>;
  }
Ref<CpuTaskCtl> createCpuTask(T &&task) {
  return createCpuTask(
      [task = std::forward<T>(task)](
          const AsyncTaskCtl &) mutable -> TaskResult { return task(); });
}

template <typename T>
  requires requires(T t) {
    { t() } -> std::same_as<void>;
  }
Ref<CpuTaskCtl> createCpuTask(T &&task) {
  return createCpuTask([task = std::forward<T>(task)](
                           const AsyncTaskCtl &ctl) mutable -> TaskResult {
    if (ctl.isCancelRequested()) {
      return TaskResult::Canceled;
    }

    task();
    return TaskResult::Complete;
  });
}

template <typename T>
  requires requires(T t, const AsyncTaskCtl &ctl) {
    { t(ctl) } -> std::same_as<void>;
  }
Ref<CpuTaskCtl> createCpuTask(T &&task) {
  return createCpuTask([task = std::forward<T>(task)](const AsyncTaskCtl &ctl) {
    if (ctl.isCancelRequested()) {
      return TaskResult::Canceled;
    }

    task(ctl);
    return TaskResult::Complete;
  });
}

class Scheduler;

class CpuTaskSet {
  std::vector<Ref<CpuTaskCtl>> tasks;

public:
  void append(Ref<CpuTaskCtl> task) { tasks.push_back(std::move(task)); }

  void wait() {
    for (auto task : tasks) {
      task->wait();
    }

    tasks.clear();
  }

  void enqueue(Scheduler &scheduler);
};

class TaskSet {
  struct TaskEntry {
    Ref<AsyncTaskCtl> ctl;
    std::function<void()> schedule;
  };

  std::vector<TaskEntry> tasks;

public:
  template <typename Scheduler, typename Task>
    requires requires(Scheduler &sched, Ref<Task> task) {
      sched.enqueue(std::move(task));
      task->wait();
      static_cast<Ref<AsyncTaskCtl>>(task);
    }
  void append(Scheduler &sched, Ref<Task> task) {
    Ref<AsyncTaskCtl> rawTask = task;
    auto schedFn = [sched = &sched, task = std::move(task)] {
      sched->enqueue(std::move(task));
    };

    tasks.push_back({
        .ctl = std::move(rawTask),
        .schedule = std::move(schedFn),
    });
  }

  void schedule() {
    for (auto &task : tasks) {
      if (auto schedule = std::exchange(task.schedule, nullptr)) {
        schedule();
      }
    }
  }

  bool isCanceled() const {
    for (auto &task : tasks) {
      if (task.ctl->isCanceled()) {
        return true;
      }
    }

    return false;
  }

  bool isComplete() const {
    for (auto &task : tasks) {
      if (!task.ctl->isComplete()) {
        return false;
      }
    }

    return true;
  }

  bool isInProgress() const {
    for (auto &task : tasks) {
      if (task.ctl->isInProgress()) {
        return true;
      }
    }

    return false;
  }

  void clear() { tasks.clear(); }

  void wait() const {
    for (auto &task : tasks) {
      assert(task.schedule == nullptr);
      task.ctl->wait();
    }
  }

  void cancel() {
    for (auto &task : tasks) {
      task.ctl->cancel();
    }
  }
};

class Scheduler {
  std::vector<std::thread> workThreads;
  std::vector<Ref<CpuTaskCtl>> tasks;
  std::vector<Ref<CpuTaskCtl>> rescheduleTasks;
  std::mutex taskMtx;
  std::condition_variable taskCv;
  std::atomic<bool> exit{false};

public:
  explicit Scheduler(std::size_t threadCount) {
    for (std::size_t i = 0; i < threadCount; ++i) {
      workThreads.push_back(std::thread{[this, i] {
        setThreadName(("CPU " + std::to_string(i)).c_str());
        entry();
      }});
    }
  }

  ~Scheduler() {
    exit = true;
    taskCv.notify_all();

    for (auto &thread : workThreads) {
      thread.join();
    }
  }

  void enqueue(Ref<CpuTaskCtl> task) {
    std::lock_guard lock(taskMtx);
    TaskState prevState = TaskState::Created;
    if (!task->stateStorage.compare_exchange_strong(
            prevState, TaskState::InProgress, std::memory_order::relaxed)) {
      util::unreachable("attempt to schedule cpu task in wrong state %u",
                        (unsigned)prevState);
    }
    tasks.push_back(std::move(task));
    taskCv.notify_one();
  }

  template <typename T>
    requires requires(T &&task) { createCpuTask(std::forward<T>(task)); }
  Ref<AsyncTaskCtl> enqueue(T &&task) {
    auto taskHandle = createCpuTask(std::forward<T>(task));
    enqueue(taskHandle);
    return taskHandle;
  }

  template <typename T>
    requires requires(T &&task) { createCpuTask(std::forward<T>(task)); }
  void enqueue(CpuTaskSet &set, T &&task) {
    auto taskCtl = enqueue(std::forward<T>(task));
    set.append(taskCtl);
  }

private:
  Ref<CpuTaskCtl> fetchTask() {
    std::unique_lock lock(taskMtx);

    while (tasks.empty()) {
      if (rescheduleTasks.empty() && tasks.empty()) {
        taskCv.wait(lock);
      }

      if (tasks.empty()) {
        std::swap(rescheduleTasks, tasks);
      }
    }

    auto result = std::move(tasks.back());
    tasks.pop_back();
    return result;
  }

  Ref<CpuTaskCtl> invokeTask(Ref<CpuTaskCtl> task) {
    switch (task->invoke()) {
    case TaskResult::Complete:
      task->stateStorage.store(TaskState::Complete, std::memory_order::relaxed);
      task->stateStorage.notify_all();
      return {};

    case TaskResult::Canceled:
      task->stateStorage.store(TaskState::Canceled, std::memory_order::relaxed);
      task->stateStorage.notify_all();
      return {};

    case TaskResult::Reschedule:
      return task;
    }

    std::abort();
  }

  void entry() {
    while (!exit.load(std::memory_order::relaxed)) {
      Ref<CpuTaskCtl> task = fetchTask();

      auto rescheduleTask = invokeTask(std::move(task));
      if (rescheduleTask == nullptr) {
        continue;
      }

      std::unique_lock lock(taskMtx);
      rescheduleTasks.push_back(std::move(rescheduleTask));
      taskCv.notify_one();
    }
  }
};

inline void CpuTaskSet::enqueue(Scheduler &scheduler) {
  for (auto task : tasks) {
    scheduler.enqueue(std::move(task));
  }
}
} // namespace amdgpu::device
