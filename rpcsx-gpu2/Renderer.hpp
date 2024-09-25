#pragma once

#include "Cache.hpp"
#include "Pipe.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace amdgpu {
void draw(GraphicsPipe &pipe, int vmId, std::uint32_t firstVertex,
          std::uint32_t vertexCount, std::uint32_t firstInstance,
          std::uint32_t instanceCount, std::uint64_t indiciesAddress,
          std::uint32_t indexCount);
void flip(Cache::Tag &cacheTag, VkCommandBuffer commandBuffer,
          VkExtent2D targetExtent, std::uint64_t address, VkImageView target,
          VkExtent2D imageExtent, CbCompSwap compSwap, TileMode tileMode,
          gnm::DataFormat dfmt, gnm::NumericFormat nfmt);
} // namespace amdgpu
