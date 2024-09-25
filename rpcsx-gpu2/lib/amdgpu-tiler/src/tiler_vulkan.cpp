#include "amdgpu/tiler_vulkan.hpp"
#include "Scheduler.hpp"
#include "amdgpu/tiler.hpp"
#include <bit>
#include <cstring>
#include <memory>
#include <vk.hpp>

#include <shaders/detiler1d.comp.h>
#include <shaders/detiler2d.comp.h>
#include <shaders/detilerLinear.comp.h>
#include <shaders/tiler1d.comp.h>
#include <shaders/tiler2d.comp.h>
#include <shaders/tilerLinear.comp.h>

struct TilerDecriptorSetLayout {
  VkDescriptorSetLayout layout;

  TilerDecriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings{{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VK_VERIFY(vkCreateDescriptorSetLayout(vk::context->device, &layoutInfo,
                                          nullptr, &layout));
  }

  ~TilerDecriptorSetLayout() {
    vkDestroyDescriptorSetLayout(vk::context->device, layout,
                                 vk::context->allocator);
  }
};

struct TilerShader {
  VkShaderEXT shader;

  TilerShader(TilerDecriptorSetLayout &setLayout,
              std::span<const std::uint32_t> spirv) {

    VkShaderCreateInfoEXT shaderInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .nextStage = 0,
        .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
        .codeSize = spirv.size_bytes(),
        .pCode = spirv.data(),
        .pName = "main",
        .setLayoutCount = 1,
        .pSetLayouts = &setLayout.layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = 0,
        .pSpecializationInfo = 0,
    };

    VK_VERIFY(vk::CreateShadersEXT(vk::context->device, 1, &shaderInfo, nullptr,
                                   &shader));
  }

  ~TilerShader() {
    vk::DestroyShaderEXT(vk::context->device, shader, vk::context->allocator);
  }
};

struct amdgpu::GpuTiler::Impl {
  TilerDecriptorSetLayout descriptorSetLayout;
  std::mutex descriptorMtx;
  VkDescriptorSet descriptorSets[4]{};
  VkDescriptorPool descriptorPool;
  std::uint32_t inUseDescriptorSets = 0;

  vk::Buffer configData;
  TilerShader detilerLinear{descriptorSetLayout, spirv_detilerLinear_comp};
  TilerShader detiler1d{descriptorSetLayout, spirv_detiler1d_comp};
  TilerShader detiler2d{descriptorSetLayout, spirv_detilerLinear_comp};
  TilerShader tilerLinear{descriptorSetLayout, spirv_tiler2d_comp};
  TilerShader tiler1d{descriptorSetLayout, spirv_tiler1d_comp};
  TilerShader tiler2d{descriptorSetLayout, spirv_tiler2d_comp};
  VkPipelineLayout pipelineLayout;

  struct Config {
    uint64_t srcAddress;
    uint64_t dstAddress;
    uint32_t dataWidth;
    uint32_t dataHeight;
    uint32_t tileMode;
    uint32_t numFragments;
    uint32_t bitsPerElement;
    uint32_t tiledSurfaceSize;
    uint32_t linearSurfaceSize;
  };

  Impl() {
    std::size_t count = 256;

    configData = vk::Buffer::Allocate(
        vk::getHostVisibleMemory(), sizeof(Config) * count,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    VkPipelineLayoutCreateInfo piplineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout.layout,
    };

    VK_VERIFY(vkCreatePipelineLayout(vk::context->device, &piplineLayoutInfo,
                                     nullptr, &pipelineLayout));

    {
      VkDescriptorPoolSize poolSizes[]{{
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
      }};

      VkDescriptorPoolCreateInfo info{
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
          .maxSets = static_cast<std::uint32_t>(std::size(descriptorSets)) * 4,
          .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
          .pPoolSizes = poolSizes,
      };

      VK_VERIFY(vkCreateDescriptorPool(
          vk::context->device, &info, vk::context->allocator, &descriptorPool));
    }

    VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout.layout,
    };
    for (std::size_t i = 0; i < std::size(descriptorSets); ++i) {
      VK_VERIFY(vkAllocateDescriptorSets(vk::context->device, &info,
                                         descriptorSets + i));
    }
  }

  ~Impl() {
    vkDestroyDescriptorPool(vk::context->device, descriptorPool,
                            vk::context->allocator);
    vkDestroyPipelineLayout(vk::context->device, pipelineLayout,
                            vk::context->allocator);
  }

  std::uint32_t allocateDescriptorSlot() {
    std::lock_guard lock(descriptorMtx);

    auto result = std::countl_one(inUseDescriptorSets);
    rx::dieIf(result >= std::size(descriptorSets),
              "out of tiler descriptor sets");
    inUseDescriptorSets |= (1 << result);

    return result;
  }

  void releaseDescriptorSlot(std::uint32_t slot) {
    std::lock_guard lock(descriptorMtx);
    inUseDescriptorSets &= ~(1u << slot);
  }
};

amdgpu::GpuTiler::GpuTiler() { mImpl = std::make_unique<Impl>(); }
amdgpu::GpuTiler::~GpuTiler() = default;

void amdgpu::GpuTiler::detile(Scheduler &scheduler,
                              const amdgpu::SurfaceInfo &info,
                              amdgpu::TileMode tileMode,
                              std::uint64_t srcTiledAddress,
                              std::uint64_t dstLinearAddress, int mipLevel,
                              int baseArray, int arrayCount) {
  auto commandBuffer = scheduler.getCommandBuffer();
  auto slot = mImpl->allocateDescriptorSlot();

  auto configOffset = slot * sizeof(Impl::Config);
  auto config = reinterpret_cast<Impl::Config *>(mImpl->configData.getData() +
                                                 configOffset);

  auto &subresource = info.getSubresourceInfo(mipLevel);
  config->srcAddress = srcTiledAddress + subresource.offset +
                       (subresource.tiledSize * baseArray);
  config->dstAddress = dstLinearAddress + (subresource.linearSize * baseArray);
  config->dataWidth = subresource.dataWidth;
  config->dataHeight = subresource.dataHeight;
  config->tileMode = tileMode.raw;
  config->numFragments = info.numFragments;
  config->bitsPerElement = info.bitsPerElement;
  uint32_t groupCountZ = subresource.dataDepth;

  if (arrayCount > 1) {
    config->tiledSurfaceSize = subresource.tiledSize;
    config->linearSurfaceSize = subresource.linearSize;
    groupCountZ = arrayCount;
  } else {
    config->tiledSurfaceSize = 0;
    config->linearSurfaceSize = 0;
  }

  VkShaderStageFlagBits stages[]{VK_SHADER_STAGE_COMPUTE_BIT};

  switch (tileMode.arrayMode()) {
  case amdgpu::kArrayModeLinearGeneral:
  case amdgpu::kArrayModeLinearAligned:
    vk::CmdBindShadersEXT(commandBuffer, 1, stages,
                          &mImpl->detilerLinear.shader);
    break;

  case amdgpu::kArrayMode1dTiledThin:
  case amdgpu::kArrayMode1dTiledThick:
    vk::CmdBindShadersEXT(commandBuffer, 1, stages, &mImpl->detiler1d.shader);
    break;

  case amdgpu::kArrayMode2dTiledThin:
  case amdgpu::kArrayModeTiledThinPrt:
  case amdgpu::kArrayMode2dTiledThinPrt:
  case amdgpu::kArrayMode2dTiledThick:
  case amdgpu::kArrayMode2dTiledXThick:
  case amdgpu::kArrayModeTiledThickPrt:
  case amdgpu::kArrayMode2dTiledThickPrt:
  case amdgpu::kArrayMode3dTiledThinPrt:
  case amdgpu::kArrayMode3dTiledThin:
  case amdgpu::kArrayMode3dTiledThick:
  case amdgpu::kArrayMode3dTiledXThick:
  case amdgpu::kArrayMode3dTiledThickPrt:
    std::abort();
    vk::CmdBindShadersEXT(commandBuffer, 1, stages, &mImpl->detiler2d.shader);
    break;
  }

  VkDescriptorBufferInfo bufferInfo{
      .buffer = mImpl->configData.getHandle(),
      .offset = configOffset,
      .range = sizeof(Impl::Config),
  };

  VkWriteDescriptorSet writeDescSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = mImpl->descriptorSets[slot],
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo = &bufferInfo,
  };

  vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mImpl->pipelineLayout, 0, 1,
                          &mImpl->descriptorSets[slot], 0, nullptr);

  vkCmdDispatch(commandBuffer, subresource.dataWidth, subresource.dataHeight,
                groupCountZ);

  scheduler.afterSubmit([this, slot] { mImpl->releaseDescriptorSlot(slot); });
}

void amdgpu::GpuTiler::tile(Scheduler &scheduler,
                            const amdgpu::SurfaceInfo &info,
                            amdgpu::TileMode tileMode,
                            std::uint64_t srcLinearAddress,
                            std::uint64_t dstTiledAddress, int mipLevel,
                            int baseArray, int arrayCount) {
  auto commandBuffer = scheduler.getCommandBuffer();
  auto slot = mImpl->allocateDescriptorSlot();

  auto configOffset = slot * sizeof(Impl::Config);
  auto config = reinterpret_cast<Impl::Config *>(mImpl->configData.getData() +
                                                 configOffset);

  auto &subresource = info.getSubresourceInfo(mipLevel);
  config->srcAddress = srcLinearAddress + subresource.offset +
                       subresource.linearSize * baseArray;
  config->dstAddress = dstTiledAddress;
  config->dataWidth = subresource.dataWidth;
  config->dataHeight = subresource.dataHeight;
  config->tileMode = tileMode.raw;
  config->numFragments = info.numFragments;
  config->bitsPerElement = info.bitsPerElement;
  uint32_t groupCountZ = subresource.dataDepth;

  if (arrayCount > 1) {
    config->tiledSurfaceSize = subresource.tiledSize;
    config->linearSurfaceSize = subresource.linearSize;
    groupCountZ = arrayCount;
  } else {
    config->tiledSurfaceSize = 0;
    config->linearSurfaceSize = 0;
  }

  VkShaderStageFlagBits stages[]{VK_SHADER_STAGE_COMPUTE_BIT};

  switch (tileMode.arrayMode()) {
  case amdgpu::kArrayModeLinearGeneral:
  case amdgpu::kArrayModeLinearAligned:
    vk::CmdBindShadersEXT(commandBuffer, 1, stages, &mImpl->tilerLinear.shader);
    break;

  case amdgpu::kArrayMode1dTiledThin:
  case amdgpu::kArrayMode1dTiledThick:
    vk::CmdBindShadersEXT(commandBuffer, 1, stages, &mImpl->tiler1d.shader);
    break;

  case amdgpu::kArrayMode2dTiledThin:
  case amdgpu::kArrayModeTiledThinPrt:
  case amdgpu::kArrayMode2dTiledThinPrt:
  case amdgpu::kArrayMode2dTiledThick:
  case amdgpu::kArrayMode2dTiledXThick:
  case amdgpu::kArrayModeTiledThickPrt:
  case amdgpu::kArrayMode2dTiledThickPrt:
  case amdgpu::kArrayMode3dTiledThinPrt:
  case amdgpu::kArrayMode3dTiledThin:
  case amdgpu::kArrayMode3dTiledThick:
  case amdgpu::kArrayMode3dTiledXThick:
  case amdgpu::kArrayMode3dTiledThickPrt:
    std::abort();
    vk::CmdBindShadersEXT(commandBuffer, 1, stages, &mImpl->tiler2d.shader);
    break;
  }

  VkDescriptorBufferInfo bufferInfo{
      .buffer = mImpl->configData.getHandle(),
      .offset = configOffset,
      .range = sizeof(Impl::Config),
  };

  VkWriteDescriptorSet writeDescSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = mImpl->descriptorSets[slot],
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo = &bufferInfo,
  };

  vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mImpl->pipelineLayout, 0, 1,
                          &mImpl->descriptorSets[slot], 0, nullptr);

  vkCmdDispatch(commandBuffer, subresource.dataWidth, subresource.dataHeight,
                groupCountZ);

  scheduler.afterSubmit([this, slot] { mImpl->releaseDescriptorSlot(slot); });
}
