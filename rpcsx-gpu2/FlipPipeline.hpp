#pragma once

#include "Scheduler.hpp"
#include <atomic>
#include <cstdint>
#include <vulkan/vulkan.h>

enum class FlipType {
  Std,
  Alt,
};

struct FlipPipeline {
  VkShaderModule flipVertShaderModule{};
  VkShaderModule flipFragStdShaderModule{};
  VkShaderModule flipFragAltShaderModule{};
  VkPipelineLayout pipelineLayout{};
  VkDescriptorSetLayout descriptorSetLayout{};
  VkPipeline pipelines[2]{};
  VkDescriptorPool descriptorPool{};
  VkDescriptorSet descriptorSets[8]{};
  std::atomic<std::uint8_t> freeDescriptorSets{0};

  FlipPipeline(const FlipPipeline &) = delete;
  FlipPipeline();
  ~FlipPipeline();

  void bind(Scheduler &sched, FlipType type, VkImageView imageView,
            VkSampler sampler);
};
