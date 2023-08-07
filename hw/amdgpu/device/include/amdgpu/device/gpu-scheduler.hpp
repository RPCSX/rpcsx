#pragma once

#include "scheduler.hpp"
#include "vk.hpp"
#include <atomic>
#include <concepts>
#include <cstdint>
#include <list>
#include <source_location>
#include <thread>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace amdgpu::device {
enum class ProcessQueue {
  Graphics = 1 << 1,
  Compute = 1 << 2,
  Transfer = 1 << 3,
  Any = Graphics | Compute | Transfer
};

inline ProcessQueue operator|(ProcessQueue lhs, ProcessQueue rhs) {
  return static_cast<ProcessQueue>(std::to_underlying(lhs) |
                                   std::to_underlying(rhs));
}

inline ProcessQueue operator&(ProcessQueue lhs, ProcessQueue rhs) {
  return static_cast<ProcessQueue>(std::to_underlying(lhs) &
                                   std::to_underlying(rhs));
}

struct TaskChain;
class GpuScheduler;

Scheduler &getCpuScheduler();
GpuScheduler &getGpuScheduler(ProcessQueue queue);

struct GpuTaskLayout {
  static constexpr auto kInvalidId = 0; //~static_cast<std::uint64_t>(0);

  Ref<TaskChain> chain;
  std::uint64_t id;
  std::uint64_t waitId = kInvalidId;
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  std::function<void(VkCommandBuffer)> invoke;
  std::function<void(VkQueue, VkCommandBuffer)> submit;
};

struct TaskChain {
  vk::Semaphore semaphore;
  std::uint64_t nextTaskId = 1;
  std::atomic<unsigned> refs{0};
  std::vector<std::source_location> taskLocations;

  void incRef() { refs.fetch_add(1, std::memory_order::relaxed); }
  void decRef() {
    if (refs.fetch_sub(1, std::memory_order::relaxed) == 1) {
      delete this;
    }
  }

  static Ref<TaskChain> Create() {
    auto result = new TaskChain();
    result->semaphore = vk::Semaphore::Create();
    return result;
  }

  std::uint64_t add(ProcessQueue queue, std::uint64_t waitId,
                    std::function<void(VkCommandBuffer)> invoke);

  std::uint64_t add(ProcessQueue queue,
                    std::function<void(VkCommandBuffer)> invoke) {
    return add(queue, GpuTaskLayout::kInvalidId, std::move(invoke));
  }

  template <typename T>
    requires requires(T &&t) {
      { t() } -> std::same_as<TaskResult>;
    }
  std::uint64_t add(std::uint64_t waitId, T &&task) {
    auto prevTaskId = getLastTaskId();
    auto id = nextTaskId++;
    enum class State {
      WaitTask,
      PrevTask,
    };
    auto cpuTask = createCpuTask([=, task = std::forward<T>(task),
                                  self = Ref(this), state = State::WaitTask](
                                     const AsyncTaskCtl &) mutable {
      if (state == State::WaitTask) {
        if (waitId != GpuTaskLayout::kInvalidId) {
          if (self->semaphore.getCounterValue() < waitId) {
            return TaskResult::Reschedule;
          }
        }

        auto result = task();

        if (result != TaskResult::Complete) {
          return result;
        }
        state = State::PrevTask;
      }

      if (state == State::PrevTask) {
        if (prevTaskId != GpuTaskLayout::kInvalidId && waitId != prevTaskId) {
          if (self->semaphore.getCounterValue() < prevTaskId) {
            return TaskResult::Reschedule;
          }
        }

        self->semaphore.signal(id);
      }

      return TaskResult::Complete;
    });
    getCpuScheduler().enqueue(std::move(cpuTask));
    return id;
  }

  template <typename T>
    requires requires(T &&t) {
      { t() } -> std::same_as<void>;
    }
  std::uint64_t add(std::uint64_t waitId, T &&task) {
    auto prevTaskId = getLastTaskId();
    auto id = nextTaskId++;
    enum class State {
      WaitTask,
      PrevTask,
    };
    auto cpuTask = createCpuTask([=, task = std::forward<T>(task),
                                  self = Ref(this), state = State::WaitTask](
                                     const AsyncTaskCtl &) mutable {
      if (state == State::WaitTask) {
        if (waitId != GpuTaskLayout::kInvalidId) {
          if (self->semaphore.getCounterValue() < waitId) {
            return TaskResult::Reschedule;
          }
        }

        task();
        state = State::PrevTask;
      }

      if (state == State::PrevTask) {
        if (prevTaskId != GpuTaskLayout::kInvalidId && waitId != prevTaskId) {
          if (self->semaphore.getCounterValue() < prevTaskId) {
            return TaskResult::Reschedule;
          }
        }

        self->semaphore.signal(id);
      }
      return TaskResult::Complete;
    });
    getCpuScheduler().enqueue(std::move(cpuTask));
    return id;
  }

  template <typename T>
    requires requires(T &&t) {
      { t() } -> std::same_as<void>;
    }
  std::uint64_t add(T &&task) {
    return add(GpuTaskLayout::kInvalidId, std::forward<T>(task));
  }

  template <typename T>
    requires requires(T &&t) {
      { t() } -> std::same_as<TaskResult>;
    }
  std::uint64_t add(T &&task) {
    return add(GpuTaskLayout::kInvalidId, std::forward<T>(task));
  }

  std::uint64_t getLastTaskId() const { return nextTaskId - 1; }

  std::uint64_t createExternalTask() { return nextTaskId++; }
  void notifyExternalTaskComplete(std::uint64_t id) { semaphore.signal(id); }

  bool isComplete() const { return isComplete(getLastTaskId()); }

  bool isComplete(std::uint64_t task) const {
    return semaphore.getCounterValue() >= task;
  }

  bool empty() const { return getLastTaskId() == GpuTaskLayout::kInvalidId; }

  void wait(std::uint64_t task = GpuTaskLayout::kInvalidId) const {
    if (empty()) {
      return;
    }

    if (task == GpuTaskLayout::kInvalidId) {
      task = getLastTaskId();
    }

    Verify() << semaphore.wait(task, UINT64_MAX);
  }
};

class GpuScheduler {
  std::list<std::thread> workThreads;
  std::vector<GpuTaskLayout> tasks;
  std::vector<GpuTaskLayout> delayedTasks;
  std::mutex taskMtx;
  std::condition_variable taskCv;
  std::atomic<bool> exit{false};
  std::string debugName;

public:
  explicit GpuScheduler(std::span<std::pair<VkQueue, std::uint32_t>> queues,
                        std::string debugName)
      : debugName(debugName) {
    for (std::size_t index = 0; auto [queue, queueFamilyIndex] : queues) {
      workThreads.push_back(std::thread{[=, this] {
        setThreadName(
            ("GPU " + std::to_string(index) + " " + debugName).c_str());
        entry(queue, queueFamilyIndex);
      }});

      ++index;
    }
  }

  ~GpuScheduler() {
    exit = true;
    taskCv.notify_all();

    for (auto &thread : workThreads) {
      thread.join();
    }
  }

  void enqueue(GpuTaskLayout &&task) {
    std::lock_guard lock(taskMtx);
    tasks.push_back(std::move(task));
    taskCv.notify_one();
  }

private:
  void submitTask(VkCommandPool pool, VkQueue queue, GpuTaskLayout &task) {
    VkCommandBuffer cmdBuffer;
    {
      VkCommandBufferAllocateInfo allocateInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
      };

      Verify() << vkAllocateCommandBuffers(vk::g_vkDevice, &allocateInfo,
                                           &cmdBuffer);

      VkCommandBufferBeginInfo beginInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };

      vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    task.invoke(cmdBuffer);

    vkEndCommandBuffer(cmdBuffer);

    if (task.submit) {
      task.submit(queue, cmdBuffer);
      return;
    }

    VkSemaphoreSubmitInfo signalSemSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = task.chain->semaphore.getHandle(),
        .value = task.id,
        .stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
    };

    VkSemaphoreSubmitInfo waitSemSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = task.chain->semaphore.getHandle(),
        .value = task.waitId,
        .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
    };

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdBuffer,
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount =
            static_cast<std::uint32_t>(task.waitId ? 1 : 0),
        .pWaitSemaphoreInfos = &waitSemSubmitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufferSubmitInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalSemSubmitInfo,
    };

    Verify() << vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE);

    // if (task.signalChain->semaphore.wait(
    //         task.id, std::chrono::duration_cast<std::chrono::nanoseconds>(
    //                      std::chrono::seconds(10))
    //                      .count())) {
    //   util::unreachable("gpu operation takes too long time. wait id = %lu\n",
    //                     task.waitId);
    // }
  }

  void entry(VkQueue queue, std::uint32_t queueFamilyIndex) {
    VkCommandPool pool;
    {
      VkCommandPoolCreateInfo poolCreateInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
          .queueFamilyIndex = queueFamilyIndex};

      Verify() << vkCreateCommandPool(vk::g_vkDevice, &poolCreateInfo,
                                      vk::g_vkAllocator, &pool);
    }

    while (!exit.load(std::memory_order::relaxed)) {
      GpuTaskLayout task;

      {
        std::unique_lock lock(taskMtx);

        while (tasks.empty()) {
          if (tasks.empty() && delayedTasks.empty()) {
            taskCv.wait(lock);
          }

          if (tasks.empty()) {
            std::swap(delayedTasks, tasks);
          }
        }

        task = std::move(tasks.back());
        tasks.pop_back();
      }

      if (task.waitId != GpuTaskLayout::kInvalidId &&
          !task.chain->isComplete(task.waitId)) {
        std::unique_lock lock(taskMtx);
        delayedTasks.push_back(std::move(task));
        taskCv.notify_one();
        continue;
      }

      submitTask(pool, queue, task);
    }

    vkDestroyCommandPool(vk::g_vkDevice, pool, vk::g_vkAllocator);
  }
};

inline std::uint64_t
TaskChain::add(ProcessQueue queue, std::uint64_t waitId,
               std::function<void(VkCommandBuffer)> invoke) {
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  if (waitId == GpuTaskLayout::kInvalidId) {
    waitId = getLastTaskId();
    waitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  }
  auto id = nextTaskId++;

  getGpuScheduler(queue).enqueue({
      .chain = Ref(this),
      .id = id,
      .waitId = waitId,
      .waitStage = waitStage,
      .invoke = std::move(invoke),
  });

  return id;
}

GpuScheduler &getTransferQueueScheduler();
GpuScheduler &getComputeQueueScheduler();
GpuScheduler &getGraphicsQueueScheduler();
} // namespace amdgpu::device
