#include "FlipPipeline.hpp"
#include "shaders/flip.vert.h"
#include "shaders/flip_alt.frag.h"
#include "shaders/flip_std.frag.h"
#include "vk.hpp"
#include <vulkan/vulkan.h>

FlipPipeline::~FlipPipeline() {
  vkDestroyPipeline(vk::context->device, pipelines[0], vk::context->allocator);
  vkDestroyPipeline(vk::context->device, pipelines[1], vk::context->allocator);
  vkDestroyPipelineLayout(vk::context->device, pipelineLayout,
                          vk::context->allocator);
  vkDestroyDescriptorPool(vk::context->device, descriptorPool,
                          vk::context->allocator);
  vkDestroyDescriptorSetLayout(vk::context->device, descriptorSetLayout,
                               vk::context->allocator);
  vkDestroyShaderModule(vk::context->device, flipVertShaderModule,
                        vk::context->allocator);
  vkDestroyShaderModule(vk::context->device, flipFragStdShaderModule,
                        vk::context->allocator);
  vkDestroyShaderModule(vk::context->device, flipFragAltShaderModule,
                        vk::context->allocator);
}

FlipPipeline::FlipPipeline() {

  VkShaderModuleCreateInfo flipVertexModuleInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = std::size(spirv_flip_vert) * sizeof(*spirv_flip_std_frag),
      .pCode = spirv_flip_vert,
  };

  VkShaderModuleCreateInfo flipFragmentStdInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = std::size(spirv_flip_std_frag) * sizeof(*spirv_flip_std_frag),
      .pCode = spirv_flip_std_frag,
  };

  VkShaderModuleCreateInfo flipFragmentAltInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = std::size(spirv_flip_alt_frag) * sizeof(*spirv_flip_std_frag),
      .pCode = spirv_flip_alt_frag,
  };

  VK_VERIFY(vkCreateShaderModule(vk::context->device, &flipVertexModuleInfo,
                                 vk::context->allocator,
                                 &flipVertShaderModule));

  VK_VERIFY(vkCreateShaderModule(vk::context->device, &flipFragmentStdInfo,
                                 vk::context->allocator,
                                 &flipFragStdShaderModule));

  VK_VERIFY(vkCreateShaderModule(vk::context->device, &flipFragmentAltInfo,
                                 vk::context->allocator,
                                 &flipFragAltShaderModule));

  {
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(bindings),
        .pBindings = bindings,
    };

    vkCreateDescriptorSetLayout(vk::context->device,
                                &descriptorSetLayoutCreateInfo,
                                vk::context->allocator, &descriptorSetLayout);
  }

  {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
    };

    VK_VERIFY(vkCreatePipelineLayout(vk::context->device,
                                     &pipelineLayoutCreateInfo,
                                     vk::context->allocator, &pipelineLayout));
  }

  {
    VkPipelineShaderStageCreateInfo stagesStd[]{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            // .pNext = &flipVertexModuleInfo,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = flipVertShaderModule,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            // .pNext = &flipFragmentStdInfo,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = flipFragStdShaderModule,
            .pName = "main",
        }};
    VkPipelineShaderStageCreateInfo stagesAlt[]{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            // .pNext = &flipVertexModuleInfo,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = flipVertShaderModule,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            // .pNext = &flipFragmentAltInfo,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = flipFragAltShaderModule,
            .pName = "main",
        }};

    VkPipelineVertexInputStateCreateInfo vertexInputState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkPipelineTessellationStateCreateInfo tessellationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
    };
    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    };

    VkSampleMask sampleMask = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineMultisampleStateCreateInfo multisampleState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .pSampleMask = &sampleMask,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    VkPipelineColorBlendAttachmentState blendAttachmentState = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachmentState};

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::size(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkPipelineRenderingCreateInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfos[]{
        {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &info,
            .stageCount = std::size(stagesStd),
            .pStages = stagesStd,
            .pVertexInputState = &vertexInputState,
            .pInputAssemblyState = &inputAssemblyState,
            .pTessellationState = &tessellationState,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizationState,
            .pMultisampleState = &multisampleState,
            .pDepthStencilState = &depthStencilState,
            .pColorBlendState = &colorBlendState,
            .pDynamicState = &dynamicState,
            .layout = pipelineLayout,
        },
        {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &info,
            .stageCount = std::size(stagesAlt),
            .pStages = stagesAlt,
            .pVertexInputState = &vertexInputState,
            .pInputAssemblyState = &inputAssemblyState,
            .pTessellationState = &tessellationState,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizationState,
            .pMultisampleState = &multisampleState,
            .pDepthStencilState = &depthStencilState,
            .pColorBlendState = &colorBlendState,
            .pDynamicState = &dynamicState,
            .layout = pipelineLayout,
        },
    };

    VK_VERIFY(vkCreateGraphicsPipelines(
        vk::context->device, VK_NULL_HANDLE, std::size(pipelines),
        pipelineCreateInfos, vk::context->allocator, pipelines));
  }

  {
    VkDescriptorPoolSize poolSizes[]{
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount =
                static_cast<std::uint32_t>(std::size(descriptorSets) * 2),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount =
                static_cast<std::uint32_t>(std::size(descriptorSets) * 2),
        }};

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = static_cast<std::uint32_t>(std::size(descriptorSets) * 2),
        .poolSizeCount = std::size(poolSizes),
        .pPoolSizes = poolSizes,
    };

    VK_VERIFY(vkCreateDescriptorPool(vk::context->device,
                                     &descriptorPoolCreateInfo,
                                     vk::context->allocator, &descriptorPool));
  }

  for (auto &set : descriptorSets) {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout,
    };

    VK_VERIFY(vkAllocateDescriptorSets(vk::context->device,
                                       &descriptorSetAllocateInfo, &set));
  }
}

void FlipPipeline::bind(Scheduler &sched, FlipType type, VkImageView imageView,
                        VkSampler sampler) {
  auto cmdBuffer = sched.getCommandBuffer();
  auto descriptorIndex = descriptorSetPool.acquire();

  sched.afterSubmit(
      [this, descriptorIndex] { descriptorSetPool.release(descriptorIndex); });

  auto descriptorSet = descriptorSets[descriptorIndex];
  VkDescriptorImageInfo imageInfo = {
      .sampler = sampler,
      .imageView = imageView,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  VkWriteDescriptorSet writeDescSets[]{
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSet,
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &imageInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSet,
          .dstBinding = 1,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
          .pImageInfo = &imageInfo,
      },
  };

  vkUpdateDescriptorSets(vk::context->device, std::size(writeDescSets),
                         writeDescSets, 0, nullptr);
  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
  vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelines[static_cast<int>(type)]);
}
