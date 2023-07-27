#pragma once

#include <atomic>
#include <bit>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace amdgpu::device {
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

enum class TaskState { InProgress, Complete, Canceled };

struct AsyncTaskCtl {
  std::atomic<unsigned> refs{0};
  std::atomic<TaskState> stateStorage{TaskState::InProgress};

  virtual ~AsyncTaskCtl() = default;

  void incRef() { refs.fetch_add(1, std::memory_order::relaxed); }
  void decRef() {
    if (refs.fetch_sub(1, std::memory_order::relaxed) == 1) {
      delete this;
    }
  }

  bool isCanceled() const {
    return stateStorage.load(std::memory_order::relaxed) == TaskState::Canceled;
  }
  bool isComplete() const {
    return stateStorage.load(std::memory_order::relaxed) == TaskState::Complete;
  }
  bool isInProgress() const {
    return stateStorage.load(std::memory_order::relaxed) ==
           TaskState::InProgress;
  }

  void cancel() {
    auto state = TaskState::InProgress;

    while (state == TaskState::InProgress) {
      if (stateStorage.compare_exchange_weak(state, TaskState::Canceled,
                                             std::memory_order::relaxed)) {
        break;
      }
    }

    stateStorage.notify_all();
  }

  void complete() {
    auto state = TaskState::InProgress;

    while (state != TaskState::Complete) {
      if (stateStorage.compare_exchange_weak(state, TaskState::Complete,
                                             std::memory_order::relaxed)) {
        break;
      }
    }

    stateStorage.notify_all();
  }

  void wait() {
    stateStorage.wait(TaskState::InProgress, std::memory_order::relaxed);
  }

  virtual void invoke() = 0;
};

namespace detail {
template <typename T>
concept LambdaWithoutClosure = requires(T t) { +t; };
}

template <typename T> struct AsyncTask;

template <typename T>
  requires(std::is_invocable_r_v<bool, T, const AsyncTaskCtl &> &&
           detail::LambdaWithoutClosure<T>)
struct AsyncTask<T> : AsyncTaskCtl {
  static constexpr bool (*fn)(const AsyncTaskCtl &) = +std::declval<T>();

  AsyncTask() = default;
  AsyncTask(T &&) {}

  void invoke() override {
    auto &base = *static_cast<const AsyncTaskCtl *>(this);

    if (fn(base)) {
      complete();
    }
  }
};

template <typename T>
  requires std::is_invocable_r_v<bool, T, const AsyncTaskCtl &>
Ref<AsyncTaskCtl> createTask(T &&task) {
  return Ref<AsyncTaskCtl>(new AsyncTask<T>(std::forward<T>(task)));
}

template <typename T>
  requires(std::is_invocable_r_v<bool, T, const AsyncTaskCtl &> &&
           !detail::LambdaWithoutClosure<T>)
struct AsyncTask<T> : AsyncTaskCtl {
  alignas(T) std::byte taskStorage[sizeof(T)];

  AsyncTask() = default;
  AsyncTask(T &&t) { new (taskStorage) T(std::forward<T>(t)); }
  AsyncTask &operator=(T &&t) {
    new (taskStorage) T(std::forward<T>(t));
    return *this;
  }

  ~AsyncTask() {
    if (isInProgress()) {
      std::bit_cast<T *>(&taskStorage)->~T();
    }
  }

  void invoke() override {
    auto &lambda = *std::bit_cast<T *>(&taskStorage);
    auto &base = *static_cast<const AsyncTaskCtl *>(this);

    if (lambda(base)) {
      complete();
    }

    std::bit_cast<T *>(&taskStorage)->~T();
  }
};

class Scheduler;
class TaskSet {
  std::vector<Ref<AsyncTaskCtl>> tasks;

public:
  void append(Ref<AsyncTaskCtl> task) { tasks.push_back(std::move(task)); }

  void wait() {
    for (auto task : tasks) {
      task->wait();
    }

    tasks.clear();
  }

  void enqueue(Scheduler &scheduler);
};

class Scheduler {
  std::vector<std::thread> workThreads;
  std::vector<Ref<AsyncTaskCtl>> tasks;
  std::mutex taskMtx;
  std::condition_variable taskCv;
  std::atomic<bool> exit{false};

public:
  explicit Scheduler(std::size_t threadCount) {
    for (std::size_t i = 0; i < threadCount; ++i) {
      workThreads.push_back(std::thread{[this] { entry(); }});
    }
  }

  ~Scheduler() {
    exit = true;
    taskCv.notify_all();

    for (auto &thread : workThreads) {
      thread.join();
    }
  }

  template <typename T>
    requires std::is_invocable_r_v<bool, T, const AsyncTaskCtl &>
  Ref<AsyncTaskCtl> enqueue(T &&task) {
    auto taskHandle = createTask(std::forward<T>(task));
    enqueue(taskHandle);
    return taskHandle;
  }

  void enqueue(Ref<AsyncTaskCtl> task) {
    std::lock_guard lock(taskMtx);
    tasks.push_back(std::move(task));
    taskCv.notify_one();
  }

  template <typename T>
    requires std::is_invocable_r_v<bool, T, const AsyncTaskCtl &>
  void enqueue(TaskSet &set, T &&task) {
    auto taskCtl = enqueue(std::forward<T>(task));
    set.append(taskCtl);
  }

private:
  void entry() {
    while (!exit.load(std::memory_order::relaxed)) {
      Ref<AsyncTaskCtl> task;

      {
        std::unique_lock lock(taskMtx);

        if (tasks.empty()) {
          taskCv.wait(lock);
        }

        if (tasks.empty()) {
          continue;
        }

        task = std::move(tasks.back());
        tasks.pop_back();
      }

      task->invoke();
    }
  }
};

inline void TaskSet::enqueue(Scheduler &scheduler) {
  for (auto task : tasks) {
    scheduler.enqueue(std::move(task));
  }
}
} // namespace amdgpu::device
