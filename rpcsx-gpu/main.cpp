#include <algorithm>
#include <amdgpu/bridge/bridge.hpp>
#include <amdgpu/device/device.hpp>
#include <chrono>
#include <cstdio>
#include <thread>
#include <util/VerifyVulkan.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <GLFW/glfw3.h> // TODO: make in optional

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

  bool enableValidation = true;

  if (enableValidation) {
    requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkApplicationInfo appInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "RPCSX",
    .pEngineName = "none",
    .apiVersion = VK_API_VERSION_1_3,
  };

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
  vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &vkPhyDeviceMemoryProperties);

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
      VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_EXT_SEPARATE_STENCIL_USAGE_EXTENSION_NAME,
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
    for (auto &propery : queueFamilyProperties) {
      propery.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
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
           .queueCount = std::min<uint32_t>(queueFamilyProperties[queueFamily]
                             .queueFamilyProperties.queueCount, defaultQueuePriorities.size()),
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

  VkPhysicalDeviceVulkan13Features features13 { 
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .maintenance4 = VK_TRUE
  };

  VkPhysicalDeviceVulkan12Features features12 { 
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = &features13,
    .storageBuffer8BitAccess = VK_TRUE,
    .uniformAndStorageBuffer8BitAccess = VK_TRUE,
    .shaderFloat16 = VK_TRUE,
    .shaderInt8 = VK_TRUE,
  };

  VkPhysicalDeviceVulkan11Features features11 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .pNext = &features12,
    .storageBuffer16BitAccess = VK_TRUE,
    .uniformAndStorageBuffer16BitAccess = VK_TRUE,
  };

  VkDeviceCreateInfo deviceCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &features11,
      .queueCreateInfoCount = static_cast<uint32_t>(requestedQueues.size()),
      .pQueueCreateInfos = requestedQueues.data(),
      .enabledExtensionCount = static_cast<uint32_t>(requestedDeviceExtensions.size()),
      .ppEnabledExtensionNames = requestedDeviceExtensions.data(),
      .pEnabledFeatures = &features2.features
  };

  VkDevice vkDevice;
  Verify() << vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo, nullptr,
                             &vkDevice);


  std::vector<VkQueue> computeQueues;
  std::vector<VkQueue> transferQueues;
  std::vector<VkQueue> graphicsQueues;
  VkQueue presentQueue = VK_NULL_HANDLE;

  for (auto &queueInfo : requestedQueues) {
    if (queueFamiliesWithComputeSupport.contains(queueInfo.queueFamilyIndex)) {
      for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount; ++queueIndex) {
        vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, queueIndex, &computeQueues.emplace_back());
      }
    }

    if (queueFamiliesWithGraphicsSupport.contains(queueInfo.queueFamilyIndex)) {
      for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount; ++queueIndex) {
        vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, queueIndex, &graphicsQueues.emplace_back());
      }
    }

    if (queueFamiliesWithTransferSupport.contains(queueInfo.queueFamilyIndex)) {
      for (uint32_t queueIndex = 0; queueIndex < queueInfo.queueCount; ++queueIndex) {
        vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, queueIndex, &transferQueues.emplace_back());
      }
    }

    if (presentQueue == VK_NULL_HANDLE && queueFamiliesWithPresentSupport.contains(queueInfo.queueFamilyIndex)) {
      vkGetDeviceQueue(vkDevice, queueInfo.queueFamilyIndex, 0, &presentQueue);
    }
  }

  Verify() << (computeQueues.size() > 1);
  Verify() << (transferQueues.size() > 0);
  Verify() << (graphicsQueues.size() > 0);
  Verify() << (presentQueue != VK_NULL_HANDLE);

  amdgpu::device::setVkDevice(vkDevice, vkPhyDeviceMemoryProperties);

  std::printf("Initialization was succesful\n");

  // TODO: open emulator shared memory
  auto bridge = amdgpu::bridge::createShmCommandBuffer(cmdBridgeName);

  amdgpu::bridge::BridgePuller bridgePuller { bridge };
  amdgpu::bridge::Command commandsBuffer[32];

  amdgpu::device::DrawContext dc{
    // TODO
  };

  amdgpu::device::AmdgpuDevice device{ dc };

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    std::size_t pulledCount = bridgePuller.pullCommands(commandsBuffer, std::size(commandsBuffer));

    if (pulledCount == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Just for testing, should be removed
      continue;
    }

    for (std::size_t i = 0; i < pulledCount; ++i) {
      // TODO: handle command
    }
  }

  amdgpu::bridge::destroyShmCommandBuffer(bridge);
  amdgpu::bridge::unlinkShm(cmdBridgeName);
  return 0;
}
