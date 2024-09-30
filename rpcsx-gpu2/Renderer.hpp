#pragma once

#include "Cache.hpp"
#include "FlipPipeline.hpp"
#include "Pipe.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace amdgpu {
void draw(GraphicsPipe &pipe, int vmId, std::uint32_t firstVertex,
          std::uint32_t vertexCount, std::uint32_t firstInstance,
          std::uint32_t instanceCount, std::uint64_t indiciesAddress,
          std::uint32_t indexCount);
void dispatch(Cache &cache, Scheduler &sched,
              Registers::ComputeConfig &computeConfig,
              std::uint32_t groupCountX, std::uint32_t groupCountY,
              std::uint32_t groupCountZ);
void flip(Cache::Tag &cacheTag, VkCommandBuffer commandBuffer,
          VkExtent2D targetExtent, std::uint64_t address, VkImageView target,
          VkExtent2D imageExtent, FlipType type, TileMode tileMode,
          gnm::DataFormat dfmt, gnm::NumericFormat nfmt);
} // namespace amdgpu
