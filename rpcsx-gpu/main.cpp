#include "amdgpu/RemoteMemory.hpp"
#include "amdgpu/device/vk.hpp"
#include <algorithm>
#include <amdgpu/bridge/bridge.hpp>
#include <amdgpu/device/device.hpp>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <span>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <util/VerifyVulkan.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <GLFW/glfw3.h> // TODO: make in optional

// TODO
extern void *g_rwMemory;
extern std::size_t g_memorySize;
extern std::uint64_t g_memoryBase;
extern amdgpu::RemoteMemory g_hostMemory;

static void usage(std::FILE *out, const char *argv0) {
  std::fprintf(out, "usage: %s [options...]\n", argv0);
  std::fprintf(out, "  options:\n");
  std::fprintf(out,
               "    --cmd-bridge <name> - setup command queue bridge name\n");
  std::fprintf(out, "    --shm <name> - setup shared memory name\n");
  std::fprintf(
      out,
      "    --gpu <index> - specify physical gpu index to use, default is 0\n");
  std::fprintf(out,
               "    --presenter <presenter mode> - set flip engine target\n");
  std::fprintf(out, "    --no-validation - disable validation layers\n");
  std::fprintf(out, "    -h, --help - show this message\n");
  std::fprintf(out, "\n");
  std::fprintf(out, "  presenter mode:\n");
  std::fprintf(out, "     window - create and use native window (default)\n");
}

enum class PresenterMode { Window };

int main(int argc, const char *argv[]) {
  if (argc == 2 && (argv[1] == std::string_view("-h") ||
                    argv[1] == std::string_view("--help"))) {
    usage(stdout, argv[0]);
    return 0;
  }

  const char *cmdBridgeName = "/rpcsx-gpu-cmds";
  const char *shmName = "/rpcsx-os-memory";
  unsigned long gpuIndex = 0;
  auto presenter = PresenterMode::Window;
  bool noValidation = false;

  for (int i = 1; i < argc; ++i) {
    if (argv[i] == std::string_view("--cmd-bridge")) {
      if (argc <= i + 1) {
        usage(stderr, argv[0]);
        return 1;
      }

      cmdBridgeName = argv[++i];
      continue;
    }

    if (argv[i] == std::string_view("--shm")) {
      if (argc <= i + 1) {
        usage(stderr, argv[0]);
        return 1;
      }
      shmName = argv[++i];
      continue;
    }

    if (argv[i] == std::string_view("--presenter")) {
      if (argc <= i + 1) {
        usage(stderr, argv[0]);
        return 1;
      }

      auto presenterText = std::string_view(argv[++i]);

      if (presenterText == "window") {
        presenter = PresenterMode::Window;
      } else {
        usage(stderr, argv[0]);
        return 1;
      }
      continue;
    }

    if (argv[i] == std::string_view("--gpu")) {
      if (argc <= i + 1) {
        usage(stderr, argv[0]);
        return 1;
      }

      char *endPtr = nullptr;
      gpuIndex = std::strtoul(argv[++i], &endPtr, 10);
      if (endPtr == nullptr || *endPtr != '\0') {
        usage(stderr, argv[0]);
        return 1;
      }

      continue;
    }

    if (argv[i] == std::string_view("--no-validation")) {
      noValidation = true;
      continue;
    }

    usage(stderr, argv[0]);
    return 1;
  }

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto window = glfwCreateWindow(1280, 720, "RPCSX", nullptr, nullptr);

  const char **glfwExtensions;
  uint32_t glfwExtensionCount = 0;

  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  auto requiredInstanceExtensions = std::vector<const char *>(
      glfwExtensions, glfwExtensions + glfwExtensionCount);

  bool enableValidation = !noValidation;

  if (enableValidation) {
    requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  uint32_t extCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
  std::vector<std::string> supportedInstanceExtensions;

  if (extCount > 0) {
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vkEnumerateInstanceExtensionProperties(
            nullptr, &extCount, &extensions.front()) == VK_SUCCESS) {
      supportedInstanceExtensions.reserve(extensions.size());
      for (VkExtensionProperties extension : extensions) {
        supportedInstanceExtensions.push_back(extension.extensionName);
      }
    }
  }

  for (const char *extension : requiredInstanceExtensions) {
    if (std::find(supportedInstanceExtensions.begin(),
                  supportedInstanceExtensions.end(),
                  extension) == supportedInstanceExtensions.end()) {
      util::unreachable("Requested instance extension '%s' is not present at "
                        "instance level",
                        extension);
    }
  }

  const char *validationLayerName = "VK_LAYER_KHRONOS_validation";

  VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "RPCSX",
      .pEngineName = "none",
      .apiVersion = VK_API_VERSION_1_3,
  };

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = NULL;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  instanceCreateInfo.enabledExtensionCount = requiredInstanceExtensions.size();
  instanceCreateInfo.ppEnabledExtensionNames =
      requiredInstanceExtensions.data();

  if (enableValidation) {
    instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
    instanceCreateInfo.enabledLayerCount = 1;
  }

  VkInstance vkInstance;
  Verify() << vkCreateInstance(&instanceCreateInfo, nullptr, &vkInstance);
  auto getVkPhyDevice = [&](unsigned index) {
    std::vector<VkPhysicalDevice> devices(index + 1);
    uint32_t count = devices.size();
    Verify() << vkEnumeratePhysicalDevices(vkInstance, &count, devices.data());
    Verify() << (index < count);
    return devices[index];
  };

  auto vkPhysicalDevice = getVkPhyDevice(gpuIndex);

  VkPhysicalDeviceProperties vkPhyDeviceProperties;
  vkGetPhysicalDeviceProperties(vkPhysicalDevice, &vkPhyDeviceProperties);
  std::printf("VK: Selected physical device is %s\n",
              vkPhyDeviceProperties.deviceName);
  VkPhysicalDeviceMemoryProperties vkPhyDeviceMemoryProperties;
  vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice,
                                      &vkPhyDeviceMemoryProperties);

  VkPhysicalDevice8BitStorageFeatures storage_8bit = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES};
  VkPhysicalDevice16BitStorageFeatures storage_16bit = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
      .pNext = &storage_8bit};
  VkPhysicalDeviceShaderFloat16Int8Features float16_int8 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES,
      .pNext = &storage_16bit};

  VkPhysicalDeviceFeatures2 features2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &float16_int8};
  vkGetPhysicalDeviceFeatures2(vkPhysicalDevice, &features2);

  Verify() << storage_8bit.uniformAndStorageBuffer8BitAccess;
  Verify() << storage_16bit.uniformAndStorageBuffer16BitAccess;
  Verify() << float16_int8.shaderFloat16;
  Verify() << float16_int8.shaderInt8;

  std::vector<std::string> vkSupportedDeviceExtensions;
  {
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &extCount,
                                         nullptr);
    if (extCount > 0) {
      std::vector<VkExtensionProperties> extensions(extCount);
      if (vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr,
                                               &extCount, extensions.data()) ==
          VK_SUCCESS) {

        vkSupportedDeviceExtensions.reserve(extCount);

        for (auto ext : extensions) {
          vkSupportedDeviceExtensions.push_back(ext.extensionName);
        }
      }
    }
  }

  auto isDeviceExtensionSupported = [&](std::string_view extension) {
    return std::find(vkSupportedDeviceExtensions.begin(),
                     vkSupportedDeviceExtensions.end(),
                     extension) != vkSupportedDeviceExtensions.end();
  };

  std::vector<const char *> requestedDeviceExtensions = {
      VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME,
      VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
      VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME,
      // VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
      // VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_EXT_SEPARATE_STENCIL_USAGE_EXTENSION_NAME,
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
  };

  if (isDeviceExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
    requestedDeviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
  }

  // for (auto extension : vkSupportedDeviceExtensions) {
  //   std::printf("supported device extension %s\n", extension.c_str());
  // }

  for (const char *requestedExtension : requestedDeviceExtensions) {
    if (!isDeviceExtensionSupported(requestedExtension)) {
      std::fprintf(
          stderr,
          "Requested device extension '%s' is not present at device level\n",
          requestedExtension);
      std::abort();
    }
  }

  std::vector<VkQueueFamilyProperties2> queueFamilyProperties;

  {
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice,
                                             &queueFamilyCount, nullptr);
    Verify() << (queueFamilyCount > 0);
    queueFamilyProperties.resize(queueFamilyCount);
    for (auto &property : queueFamilyProperties) {
      property.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(
        vkPhysicalDevice, &queueFamilyCount, queueFamilyProperties.data());
  }

  VkSurfaceKHR vkSurface;
  Verify() << glfwCreateWindowSurface(vkInstance, window, nullptr, &vkSurface);

  std::set<uint32_t> queueFamiliesWithPresentSupport;
  std::set<uint32_t> queueFamiliesWithTransferSupport;
  std::set<uint32_t> queueFamiliesWithComputeSupport;
  std::set<uint32_t> queueFamiliesWithGraphicsSupport;

  uint32_t queueFamiliesCount = 0;
  for (auto &familyProperty : queueFamilyProperties) {
    VkBool32 supportsPresent;
    if (vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice,
                                             queueFamiliesCount, vkSurface,
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

  Verify() << !queueFamiliesWithPresentSupport.empty();
  Verify() << !queueFamiliesWithTransferSupport.empty();
  Verify() << !queueFamiliesWithComputeSupport.empty();
  Verify() << !queueFamiliesWithGraphicsSupport.empty();

  std::vector<VkDeviceQueueCreateInfo> requestedQueues;

  std::vector<float> defaultQueuePriorities;
  defaultQueuePriorities.resize(8);

  for (uint32_t queueFamily = 0; queueFamily < queueFamiliesCount;
       ++queueFamily) {
    if (queueFamiliesWithGraphicsSupport.contains(queueFamily)) {
      requestedQueues.push_back(
          {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
           .queueFamilyIndex = queueFamily,
           .queueCount = 1,
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

  // try to find queue that not graphics queue
  bool requestedPresentQueue = false;
  for (auto queueFamily : queueFamiliesWithPresentSupport) {
    if (queueFamiliesWithGraphicsSupport.contains(queueFamily)) {
      continue;
    }

    bool alreadyRequested = false;

    for (auto &requested : requestedQueues) {
      if (requested.queueFamilyIndex == queueFamily) {
        alreadyRequested = true;
        break;
      }
    }

    if (!alreadyRequested) {
      requestedQueues.push_back(
          {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
           .queueFamilyIndex = queueFamily,
           .queueCount = 1,
           .pQueuePriorities = defaultQueuePriorities.data()});
    }

    requestedPresentQueue = true;
  }

  if (!requestedPresentQueue) {
    for (auto queueFamily : queueFamiliesWithPresentSupport) {
      bool alreadyRequested = false;

      for (auto &requested : requestedQueues) {
        if (requested.queueFamilyIndex == queueFamily) {
          alreadyRequested = true;
          break;
        }
      }

      if (!alreadyRequested) {
        requestedQueues.push_back(
            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
             .queueFamilyIndex = queueFamily,
             .queueCount = 1,
             .pQueuePriorities = defaultQueuePriorities.data()});
      }

      requestedPresentQueue = true;
    }
  }

  VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
      .shaderObject = VK_TRUE};

  VkPhysicalDeviceVulkan13Features phyDevFeatures13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &shaderObjectFeatures,
      .dynamicRendering = VK_TRUE,
      .maintenance4 = VK_TRUE,
  };

  VkPhysicalDeviceVulkan12Features phyDevFeatures12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &phyDevFeatures13,
      .storageBuffer8BitAccess = VK_TRUE,
      .uniformAndStorageBuffer8BitAccess = VK_TRUE,
      .shaderFloat16 = VK_TRUE,
      .shaderInt8 = VK_TRUE,
      .timelineSemaphore = VK_TRUE,
  };

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
      .enabledExtensionCount =
          static_cast<uint32_t>(requestedDeviceExtensions.size()),
      .ppEnabledExtensionNames = requestedDeviceExtensions.data(),
      .pEnabledFeatures = &features2.features};

  VkDevice vkDevice;
  Verify() << vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo, nullptr,
                             &vkDevice);
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkExtent2D swapchainExtent{};

  std::vector<VkImage> swapchainImages;

  VkFormat swapchainColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
  VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  uint32_t formatCount;
  Verify() << vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurface,
                                                   &formatCount, nullptr);
  Verify() << (formatCount > 0);

  std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
  Verify() << vkGetPhysicalDeviceSurfaceFormatsKHR(
      vkPhysicalDevice, vkSurface, &formatCount, surfaceFormats.data());

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

  auto createSwapchain = [&] {
    VkSwapchainKHR oldSwapchain = swapchain;

    VkSurfaceCapabilitiesKHR surfCaps;
    Verify() << vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice,
                                                          vkSurface, &surfCaps);
    uint32_t presentModeCount;
    Verify() << vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice, vkSurface, &presentModeCount, NULL);
    Verify() << (presentModeCount > 0);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    Verify() << vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice, vkSurface, &presentModeCount, presentModes.data());

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
    swapchainCI.surface = vkSurface;
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

    Verify() << vkCreateSwapchainKHR(vkDevice, &swapchainCI, nullptr,
                                     &swapchain);

    if (oldSwapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(vkDevice, oldSwapchain, nullptr);
    }

    uint32_t swapchainImageCount = 0;
    Verify() << vkGetSwapchainImagesKHR(vkDevice, swapchain,
                                        &swapchainImageCount, nullptr);

    swapchainImages.resize(swapchainImageCount);
    Verify() << vkGetSwapchainImagesKHR(
        vkDevice, swapchain, &swapchainImageCount, swapchainImages.data());
  };

  createSwapchain();

  std::vector<std::pair<VkQueue, unsigned>> computeQueues;
  std::vector<std::pair<VkQueue, unsigned>> transferQueues;
  std::vector<std::pair<VkQueue, unsigned>> graphicsQueues;
  VkQueue presentQueue = VK_NULL_HANDLE;

  for (auto &queueInfo : requestedQueues) {
    if (queueFamiliesWithComputeSupport.contains(queueInfo.queueFamilyIndex)) {
      for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount;
           ++queueIndex) {
        auto &[queue, index] = computeQueues.emplace_back();
        index = queueInfo.queueFamilyIndex;
        vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, queueIndex,
                         &queue);
      }
    }

    if (queueFamiliesWithGraphicsSupport.contains(queueInfo.queueFamilyIndex)) {
      for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount;
           ++queueIndex) {
        auto &[queue, index] = graphicsQueues.emplace_back();
        index = queueInfo.queueFamilyIndex;
        vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, queueIndex,
                         &queue);
      }
    }

    if (queueFamiliesWithTransferSupport.contains(queueInfo.queueFamilyIndex)) {
      for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount;
           ++queueIndex) {
        auto &[queue, index] = transferQueues.emplace_back();
        index = queueInfo.queueFamilyIndex;
        vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, queueIndex,
                         &queue);
      }
    }

    if (presentQueue == VK_NULL_HANDLE &&
        queueFamiliesWithPresentSupport.contains(queueInfo.queueFamilyIndex)) {
      vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, 0, &presentQueue);
    }
  }

  Verify() << (computeQueues.size() > 1);
  Verify() << (transferQueues.size() > 0);
  Verify() << (graphicsQueues.size() > 0);
  Verify() << (presentQueue != VK_NULL_HANDLE);

  amdgpu::device::vk::g_computeQueues = computeQueues;
  amdgpu::device::vk::g_transferQueues = transferQueues;
  amdgpu::device::vk::g_graphicsQueues = graphicsQueues;

  VkCommandPoolCreateInfo commandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = graphicsQueues.front().second,
  };

  VkCommandPool commandPool;
  Verify() << vkCreateCommandPool(vkDevice, &commandPoolCreateInfo, nullptr,
                                  &commandPool);

  amdgpu::device::DrawContext dc{
      // TODO
      .queue = graphicsQueues.front().first,
      .commandPool = commandPool,
  };

  std::vector<VkFence> inFlightFences(swapchainImages.size());

  for (auto &fence : inFlightFences) {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    Verify() << vkCreateFence(vkDevice, &fenceInfo, nullptr, &fence);
  }

  VkSemaphore presentCompleteSemaphore;
  VkSemaphore renderCompleteSemaphore;
  {
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    Verify() << vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, nullptr,
                                  &presentCompleteSemaphore);
    Verify() << vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, nullptr,
                                  &renderCompleteSemaphore);
  }

  amdgpu::device::setVkDevice(vkDevice, vkPhyDeviceMemoryProperties,
                              vkPhyDeviceProperties);

  auto bridge = amdgpu::bridge::openShmCommandBuffer(cmdBridgeName);
  if (bridge == nullptr) {
    bridge = amdgpu::bridge::createShmCommandBuffer(cmdBridgeName);
  }

  if (bridge->pullerPid > 0 && ::kill(bridge->pullerPid, 0) == 0) {
    // another instance of rpcsx-gpu on the same bridge, kill self after that

    std::fprintf(stderr, "Another instance already exists\n");
    return 1;
  }

  bridge->pullerPid = ::getpid();

  amdgpu::bridge::BridgePuller bridgePuller{bridge};
  amdgpu::bridge::Command commandsBuffer[1];

  if (!std::filesystem::exists(std::string("/dev/shm") + shmName)) {
    std::printf("Waiting for OS\n");
    while (!std::filesystem::exists(std::string("/dev/shm") + shmName)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  }

  int memoryFd = ::shm_open(shmName, O_RDWR, S_IRUSR | S_IWUSR);

  if (memoryFd < 0) {
    std::printf("failed to open shared memory\n");
    return 1;
  }

  struct stat memoryStat;
  ::fstat(memoryFd, &memoryStat);
  amdgpu::RemoteMemory memory{(char *)::mmap(
      nullptr, memoryStat.st_size, PROT_NONE, MAP_SHARED, memoryFd, 0)};

  extern void *g_rwMemory;
  g_memorySize = memoryStat.st_size;
  g_memoryBase = 0x40000;
  g_rwMemory = ::mmap(nullptr, g_memorySize, PROT_READ | PROT_WRITE, MAP_SHARED,
                      memoryFd, 0);

  g_hostMemory = memory;

  {
    amdgpu::device::AmdgpuDevice device(dc, bridgePuller.header);

    for (std::uint32_t end = bridge->memoryAreaCount, i = 0; i < end; ++i) {
      auto area = bridge->memoryAreas[i];
      device.handleProtectMemory(area.address, area.size, area.prot);
    }

    std::vector<VkCommandBuffer> presentCmdBuffers(swapchainImages.size());

    {
      VkCommandBufferAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandPool = dc.commandPool;
      allocInfo.commandBufferCount = presentCmdBuffers.size();
      vkAllocateCommandBuffers(vkDevice, &allocInfo, presentCmdBuffers.data());
    }

    std::printf("Initialization complete\n");

    uint32_t imageIndex = 0;
    bool isImageAcquired = false;
    std::vector<std::vector<VkBuffer>> swapchainBufferHandles;
    swapchainBufferHandles.resize(swapchainImages.size());
    std::vector<std::vector<VkImage>> swapchainImageHandles;
    swapchainImageHandles.resize(swapchainImages.size());

    VkPipelineStageFlags submitPipelineStages =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      std::size_t pulledCount =
          bridgePuller.pullCommands(commandsBuffer, std::size(commandsBuffer));

      if (pulledCount == 0) {
        // std::this_thread::sleep_for(
        //     std::chrono::milliseconds(1)); // Just for testing, should be
        //     removed
        continue;
      }

      for (auto cmd : std::span(commandsBuffer, pulledCount)) {
        switch (cmd.id) {
        case amdgpu::bridge::CommandId::ProtectMemory:
          device.handleProtectMemory(cmd.memoryProt.address,
                                     cmd.memoryProt.size, cmd.memoryProt.prot);
          break;
        case amdgpu::bridge::CommandId::CommandBuffer:
          device.handleCommandBuffer(cmd.commandBuffer.queue,
                                     cmd.commandBuffer.address,
                                     cmd.commandBuffer.size);
          break;
        case amdgpu::bridge::CommandId::Flip: {
          if (!isImageAcquired) {
            Verify() << vkAcquireNextImageKHR(vkDevice, swapchain, UINT64_MAX,
                                              presentCompleteSemaphore, nullptr,
                                              &imageIndex);

            vkWaitForFences(vkDevice, 1, &inFlightFences[imageIndex], VK_TRUE,
                            UINT64_MAX);
            vkResetFences(vkDevice, 1, &inFlightFences[imageIndex]);
          }

          isImageAcquired = false;

          vkResetCommandBuffer(presentCmdBuffers[imageIndex], 0);
          VkCommandBufferBeginInfo beginInfo{};
          beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
          beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

          vkBeginCommandBuffer(presentCmdBuffers[imageIndex], &beginInfo);

          for (auto handle : swapchainBufferHandles[imageIndex]) {
            vkDestroyBuffer(vkDevice, handle, nullptr);
          }

          for (auto handle : swapchainImageHandles[imageIndex]) {
            vkDestroyImage(vkDevice, handle, nullptr);
          }

          swapchainBufferHandles[imageIndex].clear();
          swapchainImageHandles[imageIndex].clear();

          if (device.handleFlip(cmd.flip.bufferIndex, cmd.flip.arg,
                                presentCmdBuffers[imageIndex],
                                swapchainImages[imageIndex], swapchainExtent,
                                swapchainBufferHandles[imageIndex],
                                swapchainImageHandles[imageIndex])) {
            vkEndCommandBuffer(presentCmdBuffers[imageIndex]);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &presentCmdBuffers[imageIndex];
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderCompleteSemaphore;
            submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
            submitInfo.pWaitDstStageMask = &submitPipelineStages;

            Verify() << vkQueueSubmit(dc.queue, 1, &submitInfo,
                                      inFlightFences[imageIndex]);

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderCompleteSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            if (vkQueuePresentKHR(presentQueue, &presentInfo) != VK_SUCCESS) {
              std::printf("swapchain was invalidated\n");
              createSwapchain();
            }
            // std::this_thread::sleep_for(std::chrono::seconds(3));
          } else {
            isImageAcquired = true;
          }

          break;
        }

        default:
          util::unreachable("Unexpected command id %u\n", (unsigned)cmd.id);
        }
      }
    }

    if (bridge->pusherPid > 0) {
      kill(bridge->pusherPid, SIGINT);
    }

    for (auto fence : inFlightFences) {
      vkDestroyFence(vkDevice, fence, nullptr);
    }

    vkDestroySemaphore(vkDevice, presentCompleteSemaphore, nullptr);
    vkDestroySemaphore(vkDevice, renderCompleteSemaphore, nullptr);
    vkDestroyCommandPool(vkDevice, commandPool, nullptr);

    for (auto &handles : swapchainImageHandles) {
      for (auto handle : handles) {
        vkDestroyImage(vkDevice, handle, nullptr);
      }
    }
    for (auto &handles : swapchainBufferHandles) {
      for (auto handle : handles) {
        vkDestroyBuffer(vkDevice, handle, nullptr);
      }
    }
  }

  vkDestroySwapchainKHR(vkDevice, swapchain, nullptr);
  vkDestroyDevice(vkDevice, nullptr);
  vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
  vkDestroyInstance(vkInstance, nullptr);

  glfwDestroyWindow(window);

  amdgpu::bridge::destroyShmCommandBuffer(bridge);
  amdgpu::bridge::unlinkShm(cmdBridgeName);
  return 0;
}
