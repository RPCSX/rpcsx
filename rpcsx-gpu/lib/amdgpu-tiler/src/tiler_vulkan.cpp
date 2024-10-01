#include "amdgpu/tiler_vulkan.hpp"
#include "Scheduler.hpp"
#include "amdgpu/tiler.hpp"
#include <cstring>
#include <memory>
#include <vk.hpp>

#include <shaders/detiler1d.comp.h>
#include <shaders/detiler2d.comp.h>
#include <shaders/detilerLinear.comp.h>
#include <shaders/tiler1d.comp.h>
#include <shaders/tiler2d.comp.h>
#include <shaders/tilerLinear.comp.h>

#include <vulkan/vulkan.h>

struct Config {
  uint64_t srcAddress;
  uint64_t srcEndAddress;
  uint64_t dstAddress;
  uint64_t dstEndAddress;
  uint32_t dataWidth;
  uint32_t dataHeight;
  uint32_t tileMode;
  uint32_t macroTileMode;
  uint32_t dfmt;
  uint32_t numFragments;
  uint32_t bitsPerElement;
  uint32_t tiledSurfaceSize;
  uint32_t linearSurfaceSize;
};

struct TilerShader {
  VkShaderEXT shader;

  TilerShader(std::span<const std::uint32_t> spirv) {
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(Config),
    };

    VkShaderCreateInfoEXT shaderInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .nextStage = 0,
        .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
        .codeSize = spirv.size_bytes(),
        .pCode = spirv.data(),
        .pName = "main",
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
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
  TilerShader detilerLinear{spirv_detilerLinear_comp};
  TilerShader detiler1d{spirv_detiler1d_comp};
  TilerShader detiler2d{spirv_detilerLinear_comp};
  TilerShader tilerLinear{spirv_tiler2d_comp};
  TilerShader tiler1d{spirv_tiler1d_comp};
  TilerShader tiler2d{spirv_tiler2d_comp};
  VkPipelineLayout pipelineLayout;

  Impl() {
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(Config),
    };

    VkPipelineLayoutCreateInfo piplineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };

    VK_VERIFY(vkCreatePipelineLayout(vk::context->device, &piplineLayoutInfo,
                                     nullptr, &pipelineLayout));
  }

  ~Impl() {
    vkDestroyPipelineLayout(vk::context->device, pipelineLayout,
                            vk::context->allocator);
  }
};

amdgpu::GpuTiler::GpuTiler() { mImpl = std::make_unique<Impl>(); }
amdgpu::GpuTiler::~GpuTiler() = default;

void amdgpu::GpuTiler::detile(Scheduler &scheduler,
                              const amdgpu::SurfaceInfo &info,
                              amdgpu::TileMode tileMode, gnm::DataFormat dfmt,
                              std::uint64_t srcTiledAddress,
                              std::uint64_t srcSize,
                              std::uint64_t dstLinearAddress,
                              std::uint64_t dstSize, int mipLevel,
                              int baseArray, int arrayCount) {
  auto commandBuffer = scheduler.getCommandBuffer();

  Config config{};
  auto &subresource = info.getSubresourceInfo(mipLevel);
  config.srcAddress = srcTiledAddress + subresource.offset;
  config.srcEndAddress = srcTiledAddress + srcSize;
  config.dstAddress = dstLinearAddress;
  config.dstEndAddress = dstLinearAddress + dstSize;
  config.dataWidth = subresource.dataWidth;
  config.dataHeight = subresource.dataHeight;
  config.tileMode = tileMode.raw;
  config.dfmt = dfmt;
  config.numFragments = info.numFragments;
  config.bitsPerElement = info.bitsPerElement;
  uint32_t groupCountZ = subresource.dataDepth;

  if (arrayCount > 1) {
    config.tiledSurfaceSize = subresource.tiledSize;
    config.linearSurfaceSize = subresource.linearSize;
    groupCountZ = arrayCount;
  } else {
    config.tiledSurfaceSize = 0;
    config.linearSurfaceSize = 0;
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
    config.macroTileMode =
        getDefaultMacroTileModes()[computeMacroTileIndex(
                                       tileMode, info.bitsPerElement,
                                       1 << info.numFragments)]
            .raw;

    vk::CmdBindShadersEXT(commandBuffer, 1, stages, &mImpl->detiler1d.shader);
    break;
  }

  vkCmdPushConstants(commandBuffer, mImpl->pipelineLayout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(config), &config);
  vkCmdDispatch(commandBuffer, subresource.dataWidth, subresource.dataHeight,
                groupCountZ);
}

void amdgpu::GpuTiler::tile(Scheduler &scheduler,
                            const amdgpu::SurfaceInfo &info,
                            amdgpu::TileMode tileMode, gnm::DataFormat dfmt,
                            std::uint64_t srcLinearAddress,
                            std::uint64_t srcSize,
                            std::uint64_t dstTiledAddress,
                            std::uint64_t dstSize, int mipLevel, int baseArray,
                            int arrayCount) {
  auto commandBuffer = scheduler.getCommandBuffer();

  Config config{};

  auto &subresource = info.getSubresourceInfo(mipLevel);
  config.srcAddress = srcLinearAddress;
  config.srcEndAddress = srcLinearAddress + srcSize;
  config.dstAddress = dstTiledAddress + subresource.offset;
  config.dstEndAddress = dstTiledAddress + dstSize;
  config.dataWidth = subresource.dataWidth;
  config.dataHeight = subresource.dataHeight;
  config.tileMode = tileMode.raw;
  config.dfmt = dfmt;
  config.numFragments = info.numFragments;
  config.bitsPerElement = info.bitsPerElement;
  uint32_t groupCountZ = subresource.dataDepth;

  if (arrayCount > 1) {
    config.tiledSurfaceSize = subresource.tiledSize;
    config.linearSurfaceSize = subresource.linearSize;
    groupCountZ = arrayCount;
  } else {
    config.tiledSurfaceSize = 0;
    config.linearSurfaceSize = 0;
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
    config.macroTileMode =
        getDefaultMacroTileModes()[computeMacroTileIndex(
                                       tileMode, info.bitsPerElement,
                                       1 << info.numFragments)]
            .raw;
    vk::CmdBindShadersEXT(commandBuffer, 1, stages, &mImpl->tiler1d.shader);
    break;
  }

  vkCmdPushConstants(commandBuffer, mImpl->pipelineLayout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(config), &config);

  vkCmdDispatch(commandBuffer, subresource.dataWidth, subresource.dataHeight,
                groupCountZ);
}
