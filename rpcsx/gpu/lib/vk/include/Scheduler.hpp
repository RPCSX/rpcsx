#pragma once

#include "vk.hpp"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>
#include <vulkan/vulkan_core.h>

class Scheduler {
  vk::Semaphore mSemaphore = vk::Semaphore::Create();
  VkQueue mQueue;
  unsigned mQueueFamily;
  vk::CommandPool mCommandPool;
  vk::CommandBuffer mCommandBuffer;
  bool mIsEmpty = false;

  std::uint64_t mNextSignal = 1;
  std::mutex mTaskMutex;
  std::condition_variable mTaskCv;
  std::map<std::uint64_t, std::vector<std::move_only_function<void()>>> mTasks;
  std::vector<std::move_only_function<void()>> mAfterSubmitTasks;

  // std::jthread mThread = std::jthread{
  //     [this](std::stop_token stopToken) { schedulerEntry(stopToken); }};

public:
  Scheduler(VkQueue queue, unsigned queueFamilyIndex)
      : mQueue(queue), mQueueFamily(queueFamilyIndex) {
    mCommandPool = vk::CommandPool::Create(queueFamilyIndex);
    mCommandBuffer = mCommandPool.createOneTimeSubmitBuffer();
  }

  ~Scheduler() {
    // mThread.request_stop();
    // mTaskCv.notify_one();
  }

  unsigned getQueueFamily() const { return mQueueFamily; }
  VkQueue getQueue() const { return mQueue; }
  VkCommandBuffer getCommandBuffer() {
    mIsEmpty = false;
    return mCommandBuffer;
  }

  Scheduler &submit() {
    if (mIsEmpty) {
      return *this;
    }
    mIsEmpty = true;

    mCommandBuffer.end();

    VkSemaphoreSubmitInfo waitSemSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = mSemaphore.getHandle(),
        .value = mNextSignal - 1,
        .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
    };

    VkSemaphoreSubmitInfo signalSemSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = mSemaphore.getHandle(),
        .value = mNextSignal,
        .stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
    };

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = mCommandBuffer,
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = mNextSignal != 1 ? 1u : 0u,
        .pWaitSemaphoreInfos = &waitSemSubmitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufferSubmitInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalSemSubmitInfo,
    };

    mCommandBuffer = mCommandPool.createOneTimeSubmitBuffer();

    wait();

    VK_VERIFY(vkQueueSubmit2(mQueue, 1, &submitInfo, VK_NULL_HANDLE));

    ++mNextSignal;

    // then([afterSubmit = std::move(mAfterSubmitTasks)] mutable {
    //   for (auto &&fn : afterSubmit) {
    //     std::move(fn)();
    //   }
    // });

    // mAfterSubmitTasks.clear();

    auto endIt = mTasks.upper_bound(mNextSignal - 1);

    if (mAfterSubmitTasks.empty() && endIt == mTasks.end()) {
      return *this;
    }

    auto afterSubmit = std::move(mAfterSubmitTasks);
    mAfterSubmitTasks.clear();

    wait();

    while (!afterSubmit.empty()) {
      auto task = std::move(afterSubmit.back());
      afterSubmit.pop_back();
      std::move(task)();
    }

    std::vector<std::move_only_function<void()>> taskList;

    for (auto it = mTasks.begin(); it != mTasks.end(); it = mTasks.erase(it)) {
      taskList.reserve(taskList.size() + it->second.size());
      for (auto &&fn : it->second) {
        taskList.push_back(std::move(fn));
      }
    }

    for (auto &&task : taskList) {
      std::move(task)();
    }

    return *this;
  }

  Scheduler &afterSubmit(std::move_only_function<void()> fn) {
    mAfterSubmitTasks.push_back(std::move(fn));
    return *this;
  }

  Scheduler &then(std::move_only_function<void()> fn) {
    // auto signalValue = mNextSignal++;
    // onComplete([this, signalValue, fn = std::move(fn)] mutable {
    //   mSemaphore.wait(signalValue - 1, UINT64_MAX);
    //   std::move(fn)();
    //   mSemaphore.signal(signalValue);
    // });
    wait();
    fn();
    return *this;
  }

  // Scheduler &onComplete(std::move_only_function<void()> fn) {
  //   std::unique_lock lock(mTaskMutex);
  //   mTasks[mNextSignal - 1].push_back(std::move(fn));
  //   mTaskCv.notify_one();
  //   return *this;
  // }

  std::uint64_t createExternalSubmit() { return mNextSignal++; }
  void wait() const { mSemaphore.wait(mNextSignal - 1, UINT64_MAX); }

  VkSemaphore getSemaphoreHandle() const { return mSemaphore.getHandle(); }

private:
  void schedulerEntry(std::stop_token stopToken) {
    std::vector<std::move_only_function<void()>> taskList;
    while (!stopToken.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::microseconds(10));

      {
        std::unique_lock lock(mTaskMutex);
        while (mTasks.empty()) {
          mTaskCv.wait(lock);

          if (stopToken.stop_requested()) {
            return;
          }
        }

        auto value = mSemaphore.getCounterValue();
        auto endIt = mTasks.upper_bound(value);

        for (auto it = mTasks.begin(); it != mTasks.end();
             it = mTasks.erase(it)) {
          taskList.reserve(taskList.size() + it->second.size());
          for (auto &&fn : it->second) {
            taskList.push_back(std::move(fn));
          }
        }
      }

      for (auto &&task : taskList) {
        std::move(task)();
      }

      taskList.clear();
    }
  }
};
