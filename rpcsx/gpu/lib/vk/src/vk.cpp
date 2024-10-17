#include "vk.hpp"
#include <algorithm>
#include <bit>
#include <cstdarg>
#include <cstdio>
#include <rx/die.hpp>
#include <vulkan/vulkan_core.h>

vk::Context *vk::context;
static vk::MemoryResource g_hostVisibleMemory;
static vk::MemoryResource g_deviceLocalMemory;

void vk::verifyFailed(VkResult result, const char *message) {
  std::fprintf(stderr, "vk verification failed: %s\n", message);

  switch (result) {
  case VK_SUCCESS:
    std::fprintf(stderr, "VK_SUCCESS\n");
    break;
  case VK_NOT_READY:
    std::fprintf(stderr, "VK_NOT_READY\n");
    break;
  case VK_TIMEOUT:
    std::fprintf(stderr, "VK_TIMEOUT\n");
    break;
  case VK_EVENT_SET:
    std::fprintf(stderr, "VK_EVENT_SET\n");
    break;
  case VK_EVENT_RESET:
    std::fprintf(stderr, "VK_EVENT_RESET\n");
    break;
  case VK_INCOMPLETE:
    std::fprintf(stderr, "VK_INCOMPLETE\n");
    break;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    std::fprintf(stderr, "VK_ERROR_OUT_OF_HOST_MEMORY\n");
    break;
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    std::fprintf(stderr, "VK_ERROR_OUT_OF_DEVICE_MEMORY\n");
    break;
  case VK_ERROR_INITIALIZATION_FAILED:
    std::fprintf(stderr, "VK_ERROR_INITIALIZATION_FAILED\n");
    break;
  case VK_ERROR_DEVICE_LOST:
    std::fprintf(stderr, "VK_ERROR_DEVICE_LOST\n");
    break;
  case VK_ERROR_MEMORY_MAP_FAILED:
    std::fprintf(stderr, "VK_ERROR_MEMORY_MAP_FAILED\n");
    break;
  case VK_ERROR_LAYER_NOT_PRESENT:
    std::fprintf(stderr, "VK_ERROR_LAYER_NOT_PRESENT\n");
    break;
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    std::fprintf(stderr, "VK_ERROR_EXTENSION_NOT_PRESENT\n");
    break;
  case VK_ERROR_FEATURE_NOT_PRESENT:
    std::fprintf(stderr, "VK_ERROR_FEATURE_NOT_PRESENT\n");
    break;
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    std::fprintf(stderr, "VK_ERROR_INCOMPATIBLE_DRIVER\n");
    break;
  case VK_ERROR_TOO_MANY_OBJECTS:
    std::fprintf(stderr, "VK_ERROR_TOO_MANY_OBJECTS\n");
    break;
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    std::fprintf(stderr, "VK_ERROR_FORMAT_NOT_SUPPORTED\n");
    break;
  case VK_ERROR_FRAGMENTED_POOL:
    std::fprintf(stderr, "VK_ERROR_FRAGMENTED_POOL\n");
    break;
  case VK_ERROR_UNKNOWN:
    std::fprintf(stderr, "VK_ERROR_UNKNOWN\n");
    break;
  case VK_ERROR_OUT_OF_POOL_MEMORY:
    std::fprintf(stderr, "VK_ERROR_OUT_OF_POOL_MEMORY\n");
    break;
  case VK_ERROR_INVALID_EXTERNAL_HANDLE:
    std::fprintf(stderr, "VK_ERROR_INVALID_EXTERNAL_HANDLE\n");
    break;
  case VK_ERROR_FRAGMENTATION:
    std::fprintf(stderr, "VK_ERROR_FRAGMENTATION\n");
    break;
  case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
    std::fprintf(stderr, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS\n");
    break;
  case VK_PIPELINE_COMPILE_REQUIRED:
    std::fprintf(stderr, "VK_PIPELINE_COMPILE_REQUIRED\n");
    break;
  case VK_ERROR_SURFACE_LOST_KHR:
    std::fprintf(stderr, "VK_ERROR_SURFACE_LOST_KHR\n");
    break;
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    std::fprintf(stderr, "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR\n");
    break;
  case VK_SUBOPTIMAL_KHR:
    std::fprintf(stderr, "VK_SUBOPTIMAL_KHR\n");
    break;
  case VK_ERROR_OUT_OF_DATE_KHR:
    std::fprintf(stderr, "VK_ERROR_OUT_OF_DATE_KHR\n");
    break;
  case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
    std::fprintf(stderr, "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR\n");
    break;
  case VK_ERROR_VALIDATION_FAILED_EXT:
    std::fprintf(stderr, "VK_ERROR_VALIDATION_FAILED_EXT\n");
    break;
  case VK_ERROR_INVALID_SHADER_NV:
    std::fprintf(stderr, "VK_ERROR_INVALID_SHADER_NV\n");
    break;
  case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
    std::fprintf(stderr, "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR\n");
    break;
  case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
    std::fprintf(stderr, "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR\n");
    break;
  case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
    std::fprintf(stderr,
                 "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR\n");
    break;
  case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
    std::fprintf(stderr, "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR\n");
    break;
  case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
    std::fprintf(stderr, "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR\n");
    break;
  case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
    std::fprintf(stderr, "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR\n");
    break;
  case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
    std::fprintf(stderr,
                 "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT\n");
    break;
  case VK_ERROR_NOT_PERMITTED_KHR:
    std::fprintf(stderr, "VK_ERROR_NOT_PERMITTED_KHR\n");
    break;
  case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
    std::fprintf(stderr, "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT\n");
    break;
  case VK_THREAD_IDLE_KHR:
    std::fprintf(stderr, "VK_THREAD_IDLE_KHR\n");
    break;
  case VK_THREAD_DONE_KHR:
    std::fprintf(stderr, "VK_THREAD_DONE_KHR\n");
    break;
  case VK_OPERATION_DEFERRED_KHR:
    std::fprintf(stderr, "VK_OPERATION_DEFERRED_KHR\n");
    break;
  case VK_OPERATION_NOT_DEFERRED_KHR:
    std::fprintf(stderr, "VK_OPERATION_NOT_DEFERRED_KHR\n");
    break;
  case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
    std::fprintf(stderr, "VK_ERROR_COMPRESSION_EXHAUSTED_EXT\n");
    break;
  case VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT:
    std::fprintf(stderr, "VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT\n");
    break;

  case VK_RESULT_MAX_ENUM:
    break;
  }

  std::abort();
}

bool vk::Context::hasDeviceExtension(std::string_view ext) {
  return std::find(deviceExtensions.begin(), deviceExtensions.end(), ext) !=
         deviceExtensions.end();
}

void vk::Context::createSwapchain() {
  uint32_t formatCount;
  VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                 &formatCount, nullptr));

  std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
  VK_VERIFY(vkGetPhysicalDeviceSurfaceFormatsKHR(
      physicalDevice, surface, &formatCount, surfaceFormats.data()));

  if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
    swapchainColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    swapchainColorSpace = surfaceFormats[0].colorSpace;
  } else {
    bool found_B8G8R8A8_UNORM = false;
    for (auto &&surfaceFormat : surfaceFormats) {
      if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
        swapchainColorFormat = surfaceFormat.format;
        swapchainColorSpace = surfaceFormat.colorSpace;
        found_B8G8R8A8_UNORM = true;
        break;
      }
    }

    if (!found_B8G8R8A8_UNORM) {
      swapchainColorFormat = surfaceFormats[0].format;
      swapchainColorSpace = surfaceFormats[0].colorSpace;
    }
  }

  recreateSwapchain();

  {
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_VERIFY(vkCreateSemaphore(device, &semaphoreCreateInfo, allocator,
                                &presentCompleteSemaphore));
    VK_VERIFY(vkCreateSemaphore(device, &semaphoreCreateInfo, allocator,
                                &renderCompleteSemaphore));
  }
}

void vk::Context::recreateSwapchain() {
  VkSwapchainKHR oldSwapchain = swapchain;

  VkSurfaceCapabilitiesKHR surfCaps;
  VK_VERIFY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                      &surfCaps));
  uint32_t presentModeCount;
  VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                      &presentModeCount, nullptr));

  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  VK_VERIFY(vkGetPhysicalDeviceSurfacePresentModesKHR(
      physicalDevice, surface, &presentModeCount, presentModes.data()));

  if (surfCaps.currentExtent.width != (uint32_t)-1) {
    swapchainExtent = surfCaps.currentExtent;
  }

  VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (std::size_t i = 0; i < presentModeCount; i++) {
    if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
      continue;
    }

    if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
      break;
    }
  }

  uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount;
  if ((surfCaps.maxImageCount > 0) &&
      (desiredNumberOfSwapchainImages > surfCaps.maxImageCount)) {
    desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
  }

  VkSurfaceTransformFlagsKHR preTransform;
  if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  } else {
    preTransform = surfCaps.currentTransform;
  }

  VkCompositeAlphaFlagBitsKHR compositeAlpha =
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  };

  for (auto &compositeAlphaFlag : compositeAlphaFlags) {
    if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
      compositeAlpha = compositeAlphaFlag;
      break;
    }
  }

  VkSwapchainCreateInfoKHR swapchainCI = {};
  swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCI.surface = surface;
  swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
  swapchainCI.imageFormat = swapchainColorFormat;
  swapchainCI.imageColorSpace = swapchainColorSpace;
  swapchainCI.imageExtent = {swapchainExtent.width, swapchainExtent.height};
  swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
  swapchainCI.imageArrayLayers = 1;
  swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainCI.queueFamilyIndexCount = 0;
  swapchainCI.presentMode = swapchainPresentMode;
  swapchainCI.oldSwapchain = oldSwapchain;
  swapchainCI.clipped = VK_TRUE;
  swapchainCI.compositeAlpha = compositeAlpha;

  if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  VK_VERIFY(vkCreateSwapchainKHR(device, &swapchainCI, allocator, &swapchain));

  if (oldSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, oldSwapchain, allocator);
  }

  uint32_t swapchainImageCount = 0;
  VK_VERIFY(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount,
                                    nullptr));

  swapchainImages.resize(swapchainImageCount);
  VK_VERIFY(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount,
                                    swapchainImages.data()));

  for (auto view : swapchainImageViews) {
    vkDestroyImageView(device, view, allocator);
  }

  swapchainImageViews.resize(swapchainImageCount);
  VkImageViewCreateInfo viewInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = swapchainColorFormat,
      .subresourceRange =
          {
              .aspectMask =
                  static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT),
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  for (std::size_t index = 0; auto &view : swapchainImageViews) {
    viewInfo.image = swapchainImages[index++];
    VK_VERIFY(vkCreateImageView(device, &viewInfo, allocator, &view));
  }
}

void vk::Context::createDevice(VkSurfaceKHR surface, int gpuIndex,
                               std::vector<const char *> requiredExtensions,
                               std::vector<const char *> optionalExtensions) {
  if (device != VK_NULL_HANDLE) {
    std::abort();
  }

  auto getVkPhyDevice = [&](unsigned index) {
    uint32_t count = 0;
    VK_VERIFY(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    rx::dieIf(index >= count, "out of physical GPU devices");
    std::vector<VkPhysicalDevice> devices(count);
    VK_VERIFY(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
    return devices[index];
  };

  physicalDevice = getVkPhyDevice(gpuIndex);

  descriptorBufferProps = {
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};

  VkPhysicalDeviceProperties2 deviceProperties{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &descriptorBufferProps};
  vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
  std::printf("VK: Selected physical device is %s\n",
              deviceProperties.properties.deviceName);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice,
                                      &physicalMemoryProperties);

  // VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBuffer = {
  //     .sType =
  //     VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
  // };

  VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR fsBarycentric = {
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,
  };

  VkPhysicalDeviceShaderObjectFeaturesEXT shaderObject = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
      .pNext = &fsBarycentric,
  };
  VkPhysicalDeviceSynchronization2Features synchronization2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
      .pNext = &shaderObject,
  };
  VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
      .pNext = &synchronization2,
  };
  VkPhysicalDeviceVulkan12Features phyDevFeatures12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &dynamicRendering,
  };
  VkPhysicalDevice8BitStorageFeatures storage_8bit = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES,
      .pNext = &phyDevFeatures12,
  };
  VkPhysicalDevice16BitStorageFeatures storage_16bit = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
      .pNext = &storage_8bit};
  VkPhysicalDeviceShaderFloat16Int8Features float16_int8 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES,
      .pNext = &storage_16bit};

  VkPhysicalDeviceFeatures2 features2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &float16_int8,
  };
  vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

  supportsBarycentric = fsBarycentric.fragmentShaderBarycentric;
  supportsInt8 =
      storage_8bit.uniformAndStorageBuffer8BitAccess && float16_int8.shaderInt8;
  supportsInt64Atomics = phyDevFeatures12.shaderBufferInt64Atomics;

  if (!fsBarycentric.fragmentShaderBarycentric) {
    shaderObject.pNext = fsBarycentric.pNext;
  }

  rx::dieIf(!storage_16bit.uniformAndStorageBuffer16BitAccess,
            "16-bit storage is unsupported by this GPU");
  rx::dieIf(!float16_int8.shaderFloat16,
            "16-bit float is unsupported by this GPU");
  rx::dieIf(!phyDevFeatures12.bufferDeviceAddress,
            "bufferDeviceAddress is unsupported by this GPU");
  rx::dieIf(!phyDevFeatures12.descriptorIndexing,
            "descriptorIndexing is unsupported by this GPU");
  rx::dieIf(!phyDevFeatures12.timelineSemaphore,
            "timelineSemaphore is unsupported by this GPU");

  rx::dieIf(!synchronization2.synchronization2,
            "synchronization2 is unsupported by this GPU");
  rx::dieIf(!dynamicRendering.dynamicRendering,
            "dynamicRendering is unsupported by this GPU");
  rx::dieIf(!shaderObject.shaderObject,
            "shaderObject is unsupported by this GPU");

  std::vector<std::string> supportedExtensions;
  {
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount,
                                         nullptr);
    if (extCount > 0) {
      std::vector<VkExtensionProperties> extensions(extCount);
      if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                               &extCount, extensions.data()) ==
          VK_SUCCESS) {

        supportedExtensions.reserve(extCount);

        for (auto ext : extensions) {
          supportedExtensions.push_back(ext.extensionName);
        }
      }
    }
  }

  auto isExtensionSupported = [&](std::string_view extension) {
    return std::find(supportedExtensions.begin(), supportedExtensions.end(),
                     extension) != supportedExtensions.end();
  };

  for (const char *ext : requiredExtensions) {
    if (!isExtensionSupported(ext)) {
      std::fprintf(stderr,
                   "Required device extension '%s' is not supported by GPU\n",
                   ext);
      std::abort();
    }
  }

  for (auto optExt : optionalExtensions) {
    if (isExtensionSupported(optExt)) {
      requiredExtensions.push_back(optExt);
    }
  }

  for (auto ext : requiredExtensions) {
    if (ext ==
        std::string_view(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)) {
      supportsNonSemanticInfo = true;
    }

    deviceExtensions.push_back(ext);
  }

  std::vector<VkQueueFamilyProperties2> queueFamilyProperties;

  {
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             nullptr);
    if (queueFamilyCount == 0) {
      std::abort();
    }
    queueFamilyProperties.resize(queueFamilyCount);
    for (auto &property : queueFamilyProperties) {
      property.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount,
                                              queueFamilyProperties.data());
  }

  std::set<uint32_t> queueFamiliesWithPresentSupport;
  std::set<uint32_t> queueFamiliesWithTransferSupport;
  std::set<uint32_t> queueFamiliesWithComputeSupport;
  std::set<uint32_t> queueFamiliesWithGraphicsSupport;

  uint32_t queueFamiliesCount = 0;
  for (auto &familyProperty : queueFamilyProperties) {
    VkBool32 supportsPresent;
    if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamiliesCount,
                                             surface,
                                             &supportsPresent) == VK_SUCCESS &&
        supportsPresent != 0) {
      queueFamiliesWithPresentSupport.insert(queueFamiliesCount);
    }

    if (familyProperty.queueFamilyProperties.queueFlags &
        VK_QUEUE_SPARSE_BINDING_BIT) {
      if (familyProperty.queueFamilyProperties.queueFlags &
          VK_QUEUE_GRAPHICS_BIT) {
        queueFamiliesWithGraphicsSupport.insert(queueFamiliesCount);
      }

      if (familyProperty.queueFamilyProperties.queueFlags &
          VK_QUEUE_COMPUTE_BIT) {
        queueFamiliesWithComputeSupport.insert(queueFamiliesCount);
      }
    }

    if (familyProperty.queueFamilyProperties.queueFlags &
        VK_QUEUE_TRANSFER_BIT) {
      queueFamiliesWithTransferSupport.insert(queueFamiliesCount);
    }

    queueFamiliesCount++;
  }

  rx::dieIf(queueFamiliesWithPresentSupport.empty(), "not found queue family with present support");
  rx::dieIf(queueFamiliesWithGraphicsSupport.empty(), "not found queue family with graphics support");

  this->surface = surface;

  std::vector<VkDeviceQueueCreateInfo> requestedQueues;
  std::vector<float> defaultQueuePriorities;
  defaultQueuePriorities.resize(32);

  for (uint32_t queueFamily = 0; queueFamily < queueFamiliesCount;
       ++queueFamily) {
    if (queueFamiliesWithGraphicsSupport.contains(queueFamily)) {
      requestedQueues.push_back(
          {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
           .queueFamilyIndex = queueFamily,
           .queueCount =
               std::min<uint32_t>(queueFamilyProperties[queueFamily]
                                      .queueFamilyProperties.queueCount,
                                  defaultQueuePriorities.size()),
           .pQueuePriorities = defaultQueuePriorities.data()});
    } else if (queueFamiliesWithComputeSupport.contains(queueFamily) ||
               queueFamiliesWithTransferSupport.contains(queueFamily)) {
      requestedQueues.push_back(
          {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
           .queueFamilyIndex = queueFamily,
           .queueCount =
               std::min<uint32_t>(queueFamilyProperties[queueFamily]
                                      .queueFamilyProperties.queueCount,
                                  defaultQueuePriorities.size()),
           .pQueuePriorities = defaultQueuePriorities.data()});
    }
  }

  VkPhysicalDeviceVulkan11Features phyDevFeatures11{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &phyDevFeatures12,
      .storageBuffer16BitAccess = VK_TRUE,
      .uniformAndStorageBuffer16BitAccess = VK_TRUE,
  };

  VkDeviceCreateInfo deviceCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &phyDevFeatures11,
      .queueCreateInfoCount = static_cast<uint32_t>(requestedQueues.size()),
      .pQueueCreateInfos = requestedQueues.data(),
      .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
      .ppEnabledExtensionNames = requiredExtensions.data(),
      .pEnabledFeatures = &features2.features,
  };

  VK_VERIFY(
      vkCreateDevice(physicalDevice, &deviceCreateInfo, allocator, &device));

  for (auto &queueInfo : requestedQueues) {
    if (queueFamiliesWithGraphicsSupport.contains(queueInfo.queueFamilyIndex) &&
        graphicsQueues.empty()) {
      for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount;
           ++queueIndex) {
        if (presentQueue == VK_NULL_HANDLE &&
            queueFamiliesWithPresentSupport.contains(
                queueInfo.queueFamilyIndex)) {
          presentQueueFamily = queueInfo.queueFamilyIndex;
          vkGetDeviceQueue(device, queueInfo.queueFamilyIndex, 0,
                           &presentQueue);

          continue;
        }

        auto &[queue, index] = graphicsQueues.emplace_back();
        index = queueInfo.queueFamilyIndex;
        vkGetDeviceQueue(device, queueInfo.queueFamilyIndex, queueIndex,
                         &queue);
        break;
      }

      continue;
    }

    if (queueFamiliesWithComputeSupport.contains(queueInfo.queueFamilyIndex)) {
      if (!queueFamiliesWithTransferSupport.contains(
              queueInfo.queueFamilyIndex)) {
        std::abort();
      }

      uint32_t queueIndex = 0;
      for (; queueIndex < queueInfo.queueCount; ++queueIndex) {
        auto &[queue, index] = computeQueues.emplace_back();
        index = queueInfo.queueFamilyIndex;
        vkGetDeviceQueue(device, queueInfo.queueFamilyIndex, queueIndex,
                         &queue);
      }

      continue;
    }
  }

  rx::dieIf(presentQueue == VK_NULL_HANDLE, "present queue not found");

  if (graphicsQueues.empty()) {
    graphicsQueues.push_back({presentQueue, presentQueueFamily});
  }
}

vk::Context vk::Context::create(std::vector<const char *> requiredLayers,
                                std::vector<const char *> optionalLayers,
                                std::vector<const char *> requiredExtensions,
                                std::vector<const char *> optionalExtensions) {
  std::vector<std::string> supportedExtensions;

  {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    if (count > 0) {
      std::vector<VkExtensionProperties> extensions(count);
      if (vkEnumerateInstanceExtensionProperties(
              nullptr, &count, extensions.data()) == VK_SUCCESS) {
        supportedExtensions.reserve(extensions.size());
        for (auto &extension : extensions) {
          supportedExtensions.push_back(extension.extensionName);
        }
      }
    }
  }

  auto isExtensionSupported = [&](std::string_view name) {
    return std::find(supportedExtensions.begin(), supportedExtensions.end(),
                     name) != supportedExtensions.end();
  };

  std::vector<std::string> supportedLayers;

  {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);

    if (count > 0) {
      std::vector<VkLayerProperties> extensions(count);
      if (vkEnumerateInstanceLayerProperties(&count, extensions.data()) ==
          VK_SUCCESS) {
        supportedLayers.reserve(extensions.size());
        for (auto &layer : extensions) {
          supportedLayers.push_back(layer.layerName);
        }
      }
    }
  }

  auto isLayerSupported = [&](std::string_view name) {
    return std::find(supportedLayers.begin(), supportedLayers.end(), name) !=
           supportedLayers.end();
  };

  for (auto extension : requiredExtensions) {
    if (!isExtensionSupported(extension)) {
      std::fprintf(stderr, "Required instance extension '%s' is not supported",
                   extension);
      std::abort();
    }
  }

  for (auto layer : requiredLayers) {
    if (!isLayerSupported(layer)) {
      std::fprintf(stderr, "Required instance layer '%s' is not supported",
                   layer);
      std::abort();
    }
  }

  for (auto extension : optionalExtensions) {
    if (isExtensionSupported(extension)) {
      requiredExtensions.push_back(extension);
    }
  }

  for (auto layer : optionalLayers) {
    if (isLayerSupported(layer)) {
      requiredLayers.push_back(layer);
    }
  }

  VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "RPCSX",
      .pEngineName = "none",
      .apiVersion = VK_API_VERSION_1_3,
  };

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  instanceCreateInfo.enabledExtensionCount = requiredExtensions.size();
  instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();
  instanceCreateInfo.ppEnabledLayerNames = requiredLayers.data();
  instanceCreateInfo.enabledLayerCount = requiredLayers.size();

  std::vector<VkValidationFeatureEnableEXT> validation_feature_enables = {
      VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
  VkValidationFeaturesEXT validationFeatures{
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .enabledValidationFeatureCount =
          static_cast<uint32_t>(validation_feature_enables.size()),
      .pEnabledValidationFeatures = validation_feature_enables.data(),
  };

  bool validationPresent =
      std::find_if(
          requiredLayers.begin(), requiredLayers.end(), [](const char *layer) {
            return layer == std::string_view("VK_LAYER_KHRONOS_validation");
          }) != requiredLayers.end();

  if (validationPresent) {
    instanceCreateInfo.pNext = &validationFeatures;
  }

  Context result;
  VK_VERIFY(vkCreateInstance(&instanceCreateInfo, nullptr, &result.instance));
  return result;
}

std::uint32_t
vk::Context::findPhysicalMemoryTypeIndex(std::uint32_t typeBits,
                                         VkMemoryPropertyFlags properties) {
  typeBits &= (1 << physicalMemoryProperties.memoryTypeCount) - 1;

  while (typeBits != 0) {
    auto typeIndex = std::countr_zero(typeBits);

    if ((physicalMemoryProperties.memoryTypes[typeIndex].propertyFlags &
         properties) == properties) {
      return typeIndex;
    }

    typeBits &= ~(1 << typeIndex);
  }

  rx::die("Failed to find memory type with properties %x", properties);
}

vk::MemoryResource &vk::getHostVisibleMemory() { return g_hostVisibleMemory; }
vk::MemoryResource &vk::getDeviceLocalMemory() { return g_deviceLocalMemory; }

static auto importDeviceVkProc(VkDevice device, const char *name) {
  auto result = vkGetDeviceProcAddr(device, name);
  rx::dieIf(result == nullptr,
            "vkGetDeviceProcAddr: failed to get address of '%s'", name);
  return result;
}

static auto importInstanceVkProc(VkInstance instance, const char *name) {
  auto result = vkGetInstanceProcAddr(instance, name);
  rx::dieIf(result == nullptr,
            "vkGetInstanceProcAddr: failed to get address of '%s'", name);
  return result;
}

VkResult vk::CreateShadersEXT(VkDevice device, uint32_t createInfoCount,
                              const VkShaderCreateInfoEXT *pCreateInfos,
                              const VkAllocationCallbacks *pAllocator,
                              VkShaderEXT *pShaders) {
  static auto fn = (PFN_vkCreateShadersEXT)importDeviceVkProc(
      context->device, "vkCreateShadersEXT");
  return fn(device, createInfoCount, pCreateInfos, pAllocator, pShaders);
}

void vk::DestroyShaderEXT(VkDevice device, VkShaderEXT shader,
                          const VkAllocationCallbacks *pAllocator) {
  static auto fn = (PFN_vkDestroyShaderEXT)importDeviceVkProc(
      context->device, "vkDestroyShaderEXT");

  fn(device, shader, pAllocator);
}

void vk::CmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount,
                           const VkShaderStageFlagBits *pStages,
                           const VkShaderEXT *pShaders) {
  static PFN_vkCmdBindShadersEXT fn =
      (PFN_vkCmdBindShadersEXT)importDeviceVkProc(context->device,
                                                  "vkCmdBindShadersEXT");

  return fn(commandBuffer, stageCount, pStages, pShaders);
}

void vk::CmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer,
                                   uint32_t firstAttachment,
                                   uint32_t attachmentCount,
                                   const VkBool32 *pColorBlendEnables) {
  static auto fn = (PFN_vkCmdSetColorBlendEnableEXT)importDeviceVkProc(
      context->device, "vkCmdSetColorBlendEnableEXT");

  return fn(commandBuffer, firstAttachment, attachmentCount,
            pColorBlendEnables);
}
void vk::CmdSetColorBlendEquationEXT(
    VkCommandBuffer commandBuffer, uint32_t firstAttachment,
    uint32_t attachmentCount,
    const VkColorBlendEquationEXT *pColorBlendEquations) {
  static auto fn = (PFN_vkCmdSetColorBlendEquationEXT)importDeviceVkProc(
      context->device, "vkCmdSetColorBlendEquationEXT");

  return fn(commandBuffer, firstAttachment, attachmentCount,
            pColorBlendEquations);
}

void vk::CmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer,
                                   VkBool32 depthClampEnable) {
  static auto fn = (PFN_vkCmdSetDepthClampEnableEXT)importDeviceVkProc(
      context->device, "vkCmdSetDepthClampEnableEXT");

  return fn(commandBuffer, depthClampEnable);
}

void vk::CmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
  static auto fn = (PFN_vkCmdSetLogicOpEXT)importDeviceVkProc(
      context->device, "vkCmdSetLogicOpEXT");

  return fn(commandBuffer, logicOp);
}

void vk::CmdSetPolygonModeEXT(VkCommandBuffer commandBuffer,
                              VkPolygonMode polygonMode) {
  static auto fn = (PFN_vkCmdSetPolygonModeEXT)importDeviceVkProc(
      context->device, "vkCmdSetPolygonModeEXT");

  return fn(commandBuffer, polygonMode);
}

void vk::CmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer,
                                   VkBool32 alphaToOneEnable) {
  static auto fn = (PFN_vkCmdSetAlphaToOneEnableEXT)importDeviceVkProc(
      context->device, "vkCmdSetAlphaToOneEnableEXT");

  return fn(commandBuffer, alphaToOneEnable);
}

void vk::CmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer,
                                VkBool32 logicOpEnable) {
  static auto fn = (PFN_vkCmdSetLogicOpEnableEXT)importDeviceVkProc(
      context->device, "vkCmdSetLogicOpEnableEXT");

  return fn(commandBuffer, logicOpEnable);
}
void vk::CmdSetRasterizationSamplesEXT(
    VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
  static auto fn = (PFN_vkCmdSetRasterizationSamplesEXT)importDeviceVkProc(
      context->device, "vkCmdSetRasterizationSamplesEXT");

  return fn(commandBuffer, rasterizationSamples);
}
void vk::CmdSetSampleMaskEXT(VkCommandBuffer commandBuffer,
                             VkSampleCountFlagBits samples,
                             const VkSampleMask *pSampleMask) {
  static auto fn = (PFN_vkCmdSetSampleMaskEXT)importDeviceVkProc(
      context->device, "vkCmdSetSampleMaskEXT");

  return fn(commandBuffer, samples, pSampleMask);
}
void vk::CmdSetTessellationDomainOriginEXT(
    VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
  static auto fn = (PFN_vkCmdSetTessellationDomainOriginEXT)importDeviceVkProc(
      context->device, "vkCmdSetTessellationDomainOriginEXT");

  return fn(commandBuffer, domainOrigin);
}
void vk::CmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer,
                                        VkBool32 alphaToCoverageEnable) {
  static auto fn = (PFN_vkCmdSetAlphaToCoverageEnableEXT)importDeviceVkProc(
      context->device, "vkCmdSetAlphaToCoverageEnableEXT");

  return fn(commandBuffer, alphaToCoverageEnable);
}
void vk::CmdSetVertexInputEXT(
    VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions) {
  static auto fn = (PFN_vkCmdSetVertexInputEXT)importDeviceVkProc(
      context->device, "vkCmdSetVertexInputEXT");

  return fn(commandBuffer, vertexBindingDescriptionCount,
            pVertexBindingDescriptions, vertexAttributeDescriptionCount,
            pVertexAttributeDescriptions);
}
void vk::CmdSetColorWriteMaskEXT(
    VkCommandBuffer commandBuffer, uint32_t firstAttachment,
    uint32_t attachmentCount, const VkColorComponentFlags *pColorWriteMasks) {
  static auto fn = (PFN_vkCmdSetColorWriteMaskEXT)importDeviceVkProc(
      context->device, "vkCmdSetColorWriteMaskEXT");

  return fn(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks);
}

void vk::GetDescriptorSetLayoutSizeEXT(VkDevice device,
                                       VkDescriptorSetLayout layout,
                                       VkDeviceSize *pLayoutSizeInBytes) {
  static auto fn = (PFN_vkGetDescriptorSetLayoutSizeEXT)importDeviceVkProc(
      context->device, "vkGetDescriptorSetLayoutSizeEXT");

  return fn(device, layout, pLayoutSizeInBytes);
}

void vk::GetDescriptorSetLayoutBindingOffsetEXT(VkDevice device,
                                                VkDescriptorSetLayout layout,
                                                uint32_t binding,
                                                VkDeviceSize *pOffset) {
  static auto fn =
      (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)importDeviceVkProc(
          context->device, "vkGetDescriptorSetLayoutBindingOffsetEXT");

  return fn(device, layout, binding, pOffset);
}
void vk::GetDescriptorEXT(VkDevice device,
                          const VkDescriptorGetInfoEXT *pDescriptorInfo,
                          size_t dataSize, void *pDescriptor) {
  static auto fn = (PFN_vkGetDescriptorEXT)importDeviceVkProc(
      context->device, "vkGetDescriptorEXT");

  return fn(device, pDescriptorInfo, dataSize, pDescriptor);
}

void vk::CmdBindDescriptorBuffersEXT(
    VkCommandBuffer commandBuffer, uint32_t bufferCount,
    const VkDescriptorBufferBindingInfoEXT *pBindingInfos) {
  static auto fn = (PFN_vkCmdBindDescriptorBuffersEXT)importDeviceVkProc(
      context->device, "vkCmdBindDescriptorBuffersEXT");

  return fn(commandBuffer, bufferCount, pBindingInfos);
}

void vk::CmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer commandBuffer,
                                          VkPipelineBindPoint pipelineBindPoint,
                                          VkPipelineLayout layout,
                                          uint32_t firstSet, uint32_t setCount,
                                          const uint32_t *pBufferIndices,
                                          const VkDeviceSize *pOffsets) {
  static auto fn = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)importDeviceVkProc(
      context->device, "vkCmdSetDescriptorBufferOffsetsEXT");

  return fn(commandBuffer, pipelineBindPoint, layout, firstSet, setCount,
            pBufferIndices, pOffsets);
}

void vk::CmdBindDescriptorBufferEmbeddedSamplersEXT(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t set) {
  static auto fn =
      (PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)importDeviceVkProc(
          context->device, "vkCmdBindDescriptorBufferEmbeddedSamplersEXT");

  return fn(commandBuffer, pipelineBindPoint, layout, set);
}

VkResult vk::CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger) {
  static auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)importInstanceVkProc(
      instance, "vkCreateDebugUtilsMessengerEXT");

  return fn(instance, pCreateInfo, pAllocator, pMessenger);
}

void vk::DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks *pAllocator) {
  static auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)importInstanceVkProc(
      instance, "vkDestroyDebugUtilsMessengerEXT");

  return fn(instance, messenger, pAllocator);
}
