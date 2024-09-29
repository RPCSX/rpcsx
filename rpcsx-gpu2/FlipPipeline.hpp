#pragma once

#include "Scheduler.hpp"
#include <rx/ConcurrentBitPool.hpp>
#include <vulkan/vulkan.h>

enum class FlipType {
  Std,
  Alt,
};

struct FlipPipeline {
  static constexpr auto kDescriptorSetCount = 16;
  VkShaderModule flipVertShaderModule{};
  VkShaderModule flipFragStdShaderModule{};
  VkShaderModule flipFragAltShaderModule{};
  VkPipelineLayout pipelineLayout{};
  VkDescriptorSetLayout descriptorSetLayout{};
  VkPipeline pipelines[2]{};
  VkDescriptorPool descriptorPool{};
  VkDescriptorSet descriptorSets[kDescriptorSetCount]{};
  rx::ConcurrentBitPool<kDescriptorSetCount, std::uint8_t> descriptorSetPool;

  FlipPipeline(const FlipPipeline &) = delete;
  FlipPipeline();
  ~FlipPipeline();

  void bind(Scheduler &sched, FlipType type, VkImageView imageView,
            VkSampler sampler);
};
