#include "vk.hpp"

#include <amdgpu/bridge/bridge.hpp>
#include <rx/MemoryTable.hpp>
#include <rx/atScopeExit.hpp>
#include <rx/die.hpp>
#include <rx/mem.hpp>

#include <shader/gcn.hpp>
#include <shader/glsl.hpp>
#include <shader/spv.hpp>
#include <vulkan/vulkan.h>

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <span>
#include <thread>
#include <unordered_map>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <GLFW/glfw3.h>
#include <gnm/pm4.hpp>
#include <vulkan/vulkan_core.h>

#include <amdgpu/tiler.hpp>
#include <shaders/rdna-semantic-spirv.hpp>

#include "Device.hpp"

static void saveImage(const char *name, const void *data, std::uint32_t width,
                      std::uint32_t height) {
  std::ofstream file(name, std::ios::out | std::ios::binary);

  file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";

  auto ptr = (unsigned int *)data;
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      file.write((char *)ptr++, 3);
    }
  }
}

void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                           VkImageLayout oldLayout, VkImageLayout newLayout,
                           const VkImageSubresourceRange &subresourceRange) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = subresourceRange;

  auto layoutToStageAccess = [](VkImageLayout layout)
      -> std::pair<VkPipelineStageFlags, VkAccessFlags> {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    case VK_IMAGE_LAYOUT_GENERAL:
      return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0};

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT};

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT};

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT};

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT};

    default:
      std::abort();
    }
  };

  auto [sourceStage, sourceAccess] = layoutToStageAccess(oldLayout);
  auto [destinationStage, destinationAccess] = layoutToStageAccess(newLayout);

  barrier.srcAccessMask = sourceAccess;
  barrier.dstAccessMask = destinationAccess;

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                           VkImageAspectFlags aspectFlags,
                           VkImageLayout oldLayout, VkImageLayout newLayout) {
  transitionImageLayout(commandBuffer, image, oldLayout, newLayout,
                        VkImageSubresourceRange{
                            .aspectMask = aspectFlags,
                            .levelCount = 1,
                            .layerCount = 1,
                        });
}

static void submit(VkQueue queue, VkCommandBuffer cmdBuffer) {
  VkSubmitInfo submit{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmdBuffer,
  };

  VK_VERIFY(vkQueueSubmit(queue, 1, &submit, nullptr));
  vkQueueWaitIdle(queue);
}

static void usage(std::FILE *out, const char *argv0) {
  std::fprintf(out, "usage: %s [options...]\n", argv0);
  std::fprintf(out, "  options:\n");
  std::fprintf(out, "  --version, -v - print version\n");
  std::fprintf(out,
               "    --cmd-bridge <name> - setup command queue bridge name\n");
  std::fprintf(out, "    --shm <name> - setup shared memory name\n");
  std::fprintf(
      out,
      "    --gpu <index> - specify physical gpu index to use, default is 0\n");
  std::fprintf(out,
               "    --presenter <presenter mode> - set flip engine target\n");
  std::fprintf(out, "    --validate - enable validation layers\n");
  std::fprintf(out, "    -h, --help - show this message\n");
  std::fprintf(out, "\n");
  std::fprintf(out, "  presenter mode:\n");
  std::fprintf(out, "     window - create and use native window (default)\n");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
  if (pCallbackData->pMessage) {
    std::println("{}", pCallbackData->pMessage);
  }
  return VK_FALSE;
}

int main(int argc, const char *argv[]) {
  const char *cmdBridgeName = "/rpcsx-gpu-cmds";
  const char *shmName = "/rpcsx-os-memory";

  unsigned long gpuIndex = 0;
  // auto presenter = PresenterMode::Window;
  bool enableValidation = false;

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
        // presenter = PresenterMode::Window;
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

    if (argv[i] == std::string_view("--validate")) {
      enableValidation = true;
      continue;
    }

    usage(stderr, argv[0]);
    return 1;
  }

  if (!rx::mem::reserve((void *)0x40000, 0x60000000000 - 0x40000)) {
    std::fprintf(stderr, "failed to reserve virtual memory\n");
    return 1;
  }

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

  int dmemFd[3];

  for (std::size_t i = 0; i < std::size(dmemFd); ++i) {
    auto path = "/dev/shm/rpcsx-dmem-" + std::to_string(i);
    if (!std::filesystem::exists(path)) {
      std::printf("Waiting for dmem %zu\n", i);
      while (!std::filesystem::exists(path)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
      }
    }

    dmemFd[i] = ::shm_open(("/rpcsx-dmem-" + std::to_string(i)).c_str(), O_RDWR,
                           S_IRUSR | S_IWUSR);

    if (dmemFd[i] < 0) {
      std::printf("failed to open dmem shared memory %zu\n", i);
      return 1;
    }
  }

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto window = glfwCreateWindow(1920, 1080, "RPCSX", nullptr, nullptr);

  rx::atScopeExit _{[window] { glfwDestroyWindow(window); }};

  const char **glfwExtensions;
  uint32_t glfwExtensionCount = 0;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char *> requiredExtensions(
      glfwExtensions, glfwExtensions + glfwExtensionCount);

  std::vector<const char *> optionalLayers;

  if (enableValidation) {
    optionalLayers.push_back("VK_LAYER_KHRONOS_validation");
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  auto vkContext =
      vk::Context::create({}, optionalLayers, requiredExtensions, {});
  vk::context = &vkContext;

  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

  if (enableValidation) {
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
        .pfnUserCallback = debugUtilsMessageCallback,
    };

    VK_VERIFY(vk::CreateDebugUtilsMessengerEXT(
        vkContext.instance, &debugUtilsMessengerCreateInfo,
        vk::context->allocator, &debugMessenger));
  }

  rx::atScopeExit _debugMessenger{[=] {
    if (debugMessenger != VK_NULL_HANDLE) {
      vk::DestroyDebugUtilsMessengerEXT(vk::context->instance, debugMessenger,
                                        vk::context->allocator);
    }
  }};

  VkSurfaceKHR vkSurface;
  glfwCreateWindowSurface(vkContext.instance, window, nullptr, &vkSurface);

  vkContext.createDevice(vkSurface, gpuIndex,
                         {
                             // VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME,
                             // VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
                             // VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME,
                             // VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
                             // VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
                             // VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                             VK_EXT_SEPARATE_STENCIL_USAGE_EXTENSION_NAME,
                             VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                             VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
                             VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                             VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                             VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
                         },
                         {VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME});

  auto getTotalMemorySize = [&](int memoryType) -> VkDeviceSize {
    auto deviceLocalMemoryType =
        vkContext.findPhysicalMemoryTypeIndex(~0, memoryType);

    if (deviceLocalMemoryType < 0) {
      return 0;
    }

    auto heapIndex =
        vkContext.physicalMemoryProperties.memoryTypes[deviceLocalMemoryType]
            .heapIndex;

    return vkContext.physicalMemoryProperties.memoryHeaps[heapIndex].size;
  };

  auto localMemoryTotalSize =
      getTotalMemorySize(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  auto hostVisibleMemoryTotalSize =
      getTotalMemorySize(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  vk::getHostVisibleMemory().initHostVisible(
      std::min(hostVisibleMemoryTotalSize / 2, 1ul * 1024 * 1024 * 1024));
  vk::getDeviceLocalMemory().initDeviceLocal(
      std::min(localMemoryTotalSize / 2, 4ul * 1024 * 1024 * 1024));

  auto commandPool =
      vk::CommandPool::Create(vkContext.presentQueueFamily,
                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  vkContext.createSwapchain();
  std::vector<vk::CommandBuffer> presentCmdBuffers(
      vkContext.swapchainImages.size());

  for (auto &cmdBuffer : presentCmdBuffers) {
    cmdBuffer = commandPool.createPrimaryBuffer({});
  }

  amdgpu::bridge::BridgePuller bridgePuller{bridge};
  amdgpu::bridge::Command commandsBuffer[1];

  amdgpu::Device device;
  device.bridge = bridge;

  for (int i = 0; i < std::size(device.dmemFd); ++i) {
    device.dmemFd[i] = dmemFd[i];
  }

  uint32_t imageIndex = 0;
  bool isImageAcquired = false;
  uint32_t gpIndex = -1;
  GLFWgamepadstate gpState;

  rx::atScopeExit __{[] {
    vk::getHostVisibleMemory().free();
    vk::getDeviceLocalMemory().free();
  }};

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    while (true) {
      bool allProcessed = false;

      for (int i = 0; i < 1000; ++i) {
        if (device.processPipes()) {
          allProcessed = true;
          break;
        }
      }

      if (allProcessed) {
        break;
      }

      glfwPollEvents();

      if (glfwWindowShouldClose(window)) {
        break;
      }
    }

    std::size_t pulledCount =
        bridgePuller.pullCommands(commandsBuffer, std::size(commandsBuffer));

    if (gpIndex > GLFW_JOYSTICK_LAST) {
      for (int i = 0; i <= GLFW_JOYSTICK_LAST; ++i) {
        if (glfwJoystickIsGamepad(i) == GLFW_TRUE) {
          std::printf("Gamepad \"%s\" activated", glfwGetGamepadName(i));
          gpIndex = i;
          break;
        }
      }
    } else if (gpIndex <= GLFW_JOYSTICK_LAST) {
      if (!glfwJoystickIsGamepad(gpIndex)) {
        gpIndex = -1;
      }
    }

    if (gpIndex <= GLFW_JOYSTICK_LAST) {
      if (glfwGetGamepadState(gpIndex, &gpState) == GLFW_TRUE) {
        bridge->kbPadState.leftStickX =
            gpState.axes[GLFW_GAMEPAD_AXIS_LEFT_X] * 127.5f + 127.5f;
        bridge->kbPadState.leftStickY =
            gpState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] * 127.5f + 127.5f;
        bridge->kbPadState.rightStickX =
            gpState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] * 127.5f + 127.5f;
        bridge->kbPadState.rightStickY =
            gpState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] * 127.5f + 127.5f;
        bridge->kbPadState.l2 =
            (gpState.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 127.5f;
        bridge->kbPadState.r2 =
            (gpState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 127.5f;
        bridge->kbPadState.buttons = 0;

        if (bridge->kbPadState.l2 == 0xFF) {
          bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnL2;
        }

        if (bridge->kbPadState.r2 == 0xFF) {
          bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnR2;
        }

        static const uint32_t gpmap[GLFW_GAMEPAD_BUTTON_LAST + 1] = {
            [GLFW_GAMEPAD_BUTTON_A] = amdgpu::bridge::kPadBtnCross,
            [GLFW_GAMEPAD_BUTTON_B] = amdgpu::bridge::kPadBtnCircle,
            [GLFW_GAMEPAD_BUTTON_X] = amdgpu::bridge::kPadBtnSquare,
            [GLFW_GAMEPAD_BUTTON_Y] = amdgpu::bridge::kPadBtnTriangle,
            [GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = amdgpu::bridge::kPadBtnL1,
            [GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = amdgpu::bridge::kPadBtnR1,
            [GLFW_GAMEPAD_BUTTON_BACK] = 0,
            [GLFW_GAMEPAD_BUTTON_START] = amdgpu::bridge::kPadBtnOptions,
            [GLFW_GAMEPAD_BUTTON_GUIDE] = 0,
            [GLFW_GAMEPAD_BUTTON_LEFT_THUMB] = amdgpu::bridge::kPadBtnL3,
            [GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] = amdgpu::bridge::kPadBtnR3,
            [GLFW_GAMEPAD_BUTTON_DPAD_UP] = amdgpu::bridge::kPadBtnUp,
            [GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = amdgpu::bridge::kPadBtnRight,
            [GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = amdgpu::bridge::kPadBtnDown,
            [GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = amdgpu::bridge::kPadBtnLeft};

        for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
          if (gpState.buttons[i] == GLFW_PRESS) {
            bridge->kbPadState.buttons |= gpmap[i];
          }
        }
      }
    } else {
      bridge->kbPadState.leftStickX = 0x80;
      bridge->kbPadState.leftStickY = 0x80;
      bridge->kbPadState.rightStickX = 0x80;
      bridge->kbPadState.rightStickY = 0x80;
      bridge->kbPadState.buttons = 0;

      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        bridge->kbPadState.leftStickX = 0;
      } else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        bridge->kbPadState.leftStickX = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        bridge->kbPadState.leftStickY = 0;
      } else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        bridge->kbPadState.leftStickY = 0xff;
      }

      if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
        bridge->kbPadState.rightStickX = 0;
      } else if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        bridge->kbPadState.rightStickX = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        bridge->kbPadState.rightStickY = 0;
      } else if (glfwGetKey(window, GLFW_KEY_SEMICOLON) == GLFW_PRESS) {
        bridge->kbPadState.rightStickY = 0xff;
      }

      if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnUp;
      }
      if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnDown;
      }
      if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnLeft;
      }
      if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnRight;
      }
      if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnSquare;
      }
      if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnCross;
      }
      if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnCircle;
      }
      if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnTriangle;
      }

      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnL1;
      }
      if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnL2;
        bridge->kbPadState.l2 = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnL3;
      }
      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnPs;
      }
      if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnR1;
      }
      if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnR2;
        bridge->kbPadState.r2 = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_APOSTROPHE) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnR3;
      }

      if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        bridge->kbPadState.buttons |= amdgpu::bridge::kPadBtnOptions;
      }
    }

    bridge->kbPadState.timestamp =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();

    if (pulledCount == 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(1));
      continue;
    }

    for (auto cmd : std::span(commandsBuffer, pulledCount)) {
      switch (cmd.id) {
      case amdgpu::bridge::CommandId::ProtectMemory: {
        device.protectMemory(cmd.memoryProt.pid, cmd.memoryProt.address,
                             cmd.memoryProt.size, cmd.memoryProt.prot);
        break;
      }
      case amdgpu::bridge::CommandId::CommandBuffer: {
        device.onCommandBuffer(cmd.commandBuffer.pid, cmd.commandBuffer.queue,
                               cmd.commandBuffer.address,
                               cmd.commandBuffer.size);

        break;
      }

      case amdgpu::bridge::CommandId::Flip: {
        if (!isImageAcquired) {
          vkWaitForFences(vkContext.device, 1,
                          &vkContext.inFlightFences[imageIndex], VK_TRUE,
                          UINT64_MAX);

          while (true) {
            auto acquireNextImageResult = vkAcquireNextImageKHR(
                vkContext.device, vkContext.swapchain, UINT64_MAX,
                vkContext.presentCompleteSemaphore, nullptr, &imageIndex);
            if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
              vkContext.recreateSwapchain();
              continue;
            }

            VK_VERIFY(acquireNextImageResult);
            break;
          }

          vkResetFences(vkContext.device, 1,
                        &vkContext.inFlightFences[imageIndex]);
        }

        vkResetCommandBuffer(presentCmdBuffers[imageIndex], 0);

        if (!device.flip(cmd.flip.pid, cmd.flip.bufferIndex, cmd.flip.arg,
                         presentCmdBuffers[imageIndex],
                         vkContext.swapchainImages[imageIndex],
                         vkContext.swapchainImageViews[imageIndex],
                         vkContext.inFlightFences[imageIndex])) {
          isImageAcquired = true;
          break;
        }

        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &vkContext.renderCompleteSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &vkContext.swapchain,
            .pImageIndices = &imageIndex,
        };

        auto vkQueuePresentResult =
            vkQueuePresentKHR(vkContext.presentQueue, &presentInfo);

        if (vkQueuePresentResult == VK_ERROR_OUT_OF_DATE_KHR) {
          vkContext.recreateSwapchain();
        } else {
          VK_VERIFY(vkQueuePresentResult);
        }
        break;
      }

      case amdgpu::bridge::CommandId::MapProcess:
        device.mapProcess(cmd.mapProcess.pid, cmd.mapProcess.vmId, shmName);
        break;

      case amdgpu::bridge::CommandId::UnmapProcess:
        device.unmapProcess(cmd.mapProcess.pid);
        break;

      case amdgpu::bridge::CommandId::MapMemory:
        device.mapMemory(cmd.mapMemory.pid, cmd.mapMemory.address,
                         cmd.mapMemory.size, cmd.mapMemory.memoryType,
                         cmd.mapMemory.dmemIndex, cmd.mapMemory.prot,
                         cmd.mapMemory.offset);
        break;

      case amdgpu::bridge::CommandId::RegisterBuffer:
        device.registerBuffer(cmd.buffer.pid, cmd.buffer);
        break;

      case amdgpu::bridge::CommandId::RegisterBufferAttribute:
        device.registerBufferAttribute(cmd.bufferAttribute.pid,
                                       cmd.bufferAttribute);
        break;

      default:
        rx::die("Unexpected command id %u\n", (unsigned)cmd.id);
      }
    }
  }
}
