#include "Device.hpp"
#include "FlipPipeline.hpp"
#include "Renderer.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/constants.hpp"
#include "gnm/pm4.hpp"
#include "orbis-config.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/note.hpp"
#include "rx/AddressRange.hpp"
#include "rx/Config.hpp"
#include "rx/bits.hpp"
#include "rx/die.hpp"
#include "rx/mem.hpp"
#include "rx/watchdog.hpp"
#include "shader/spv.hpp"
#include "shaders/rdna-semantic-spirv.hpp"
#include "vk.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <format>
#include <print>
#include <stop_token>
#include <sys/mman.h>
#include <thread>

using namespace amdgpu;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
  if (pCallbackData->pMessage) {
    std::println("{}", pCallbackData->pMessage);
  }

  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    // std::abort();
  }
  return VK_FALSE;
}

enum class DisplayEvent : std::uint16_t {
  Flip = 6,
  VBlank = 7,
  PreVBlankStart = 0x59,
};

static constexpr std::uint64_t
makeDisplayEvent(DisplayEvent id, std::uint16_t unk0 = 0,
                 std::uint32_t unk1 = 0x1000'0000) {
  std::uint64_t result = 0;
  result |= static_cast<std::uint64_t>(id) << 48;
  result |= static_cast<std::uint64_t>(unk0) << 32;
  result |= static_cast<std::uint64_t>(unk1);
  return result;
}

static vk::Context createVkContext(Device *device) {
  std::vector<const char *> optionalLayers;
  bool enableValidation = rx::g_config.validateGpu;

  for (std::size_t process = 0; process < 6; ++process) {
    if (!rx::mem::reserve(
            reinterpret_cast<void *>(orbis::kMinAddress +
                                     orbis::kMaxAddress * process),
            orbis::kMaxAddress - orbis::kMinAddress)) {
      rx::die("failed to reserve userspace memory");
    }
  }

  auto createWindow = [=] {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    device->window = glfwCreateWindow(1920, 1080, "RPCSX", nullptr, nullptr);
  };

#ifdef GLFW_PLATFORM_WAYLAND
  if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND)) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
  }

  glfwInit();
  createWindow();

  if (device->window == nullptr) {
    glfwTerminate();

    glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
    glfwInit();
    createWindow();
  }
#else
  glfwInit();
  createWindow();
#endif

  glfwHideWindow(device->window);

  const char **glfwExtensions;
  uint32_t glfwExtensionCount = 0;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char *> requiredExtensions{
      glfwExtensions, glfwExtensions + glfwExtensionCount};
  if (enableValidation) {
    optionalLayers.push_back("VK_LAYER_KHRONOS_validation");
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  vk::Context result =
      vk::Context::create({}, optionalLayers, requiredExtensions, {});
  vk::context = &result;

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
        result.instance, &debugUtilsMessengerCreateInfo, vk::context->allocator,
        &device->debugMessenger));
  }

  glfwCreateWindowSurface(vk::context->instance, device->window, nullptr,
                          &device->surface);

  result.createDevice(device->surface, rx::g_config.gpuIndex,
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
                      },
                      {
                          VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
                          VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
                      });

  auto getTotalMemorySize = [&](int memoryType) -> VkDeviceSize {
    auto deviceLocalMemoryType =
        result.findPhysicalMemoryTypeIndex(~0, memoryType);

    if (deviceLocalMemoryType < 0) {
      return 0;
    }

    auto heapIndex =
        result.physicalMemoryProperties.memoryTypes[deviceLocalMemoryType]
            .heapIndex;

    return result.physicalMemoryProperties.memoryHeaps[heapIndex].size;
  };

  auto localMemoryTotalSize =
      getTotalMemorySize(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  auto hostVisibleMemoryTotalSize =
      getTotalMemorySize(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  vk::getHostVisibleMemory().initHostVisible(
      std::min(hostVisibleMemoryTotalSize / 2, 1ul * 1024 * 1024 * 1024));
  vk::getDeviceLocalMemory().initDeviceLocal(
      std::min(localMemoryTotalSize / 4, 4ul * 1024 * 1024 * 1024));

  vk::context = &device->vkContext;
  return result;
}

const auto kCachePageSize = 0x100'0000'0000 / rx::mem::pageSize;

Device::Device() : vkContext(createVkContext(this)) {
  if (!shader::spv::validate(g_rdna_semantic_spirv)) {
    shader::spv::dump(g_rdna_semantic_spirv, true);
    rx::die("builtin semantic validation failed");
  }

  if (auto sem = shader::spv::deserialize(
          shaderSemanticContext, g_rdna_semantic_spirv,
          shaderSemanticContext.getUnknownLocation())) {
    auto shaderSemantic = *sem;
    shader::gcn::canonicalizeSemantic(shaderSemanticContext, shaderSemantic);
    shader::gcn::collectSemanticModuleInfo(gcnSemanticModuleInfo,
                                           shaderSemantic);
    gcnSemantic = shader::gcn::collectSemanticInfo(gcnSemanticModuleInfo);
  } else {
    rx::die("failed to deserialize builtin semantics\n");
  }

  for (auto &pipe : graphicsPipes) {
    pipe.device = this;
  }

  for (auto &cachePage : cachePages) {
    cachePage = static_cast<std::atomic<std::uint8_t> *>(
        orbis::kalloc(kCachePageSize, 1));
    std::memset(cachePage, 0, kCachePageSize);
  }

  cacheUpdateThread = std::jthread([this](const std::stop_token &stopToken) {
    auto &sched = graphicsPipes[0].scheduler;
    std::uint32_t prevIdleValue = 0;
    while (!stopToken.stop_requested()) {
      if (gpuCacheCommandIdle.wait(prevIdleValue) != std::errc{}) {
        continue;
      }

      prevIdleValue = gpuCacheCommandIdle.load(std::memory_order::acquire);

      for (int vmId = 0; vmId < kMaxProcessCount; ++vmId) {
        auto page = gpuCacheCommand[vmId].load(std::memory_order::relaxed);
        if (page == 0) {
          continue;
        }

        gpuCacheCommand[vmId].store(0, std::memory_order::relaxed);
        auto address = static_cast<std::uint64_t>(page) * rx::mem::pageSize;

        auto range =
            rx::AddressRange::fromBeginSize(address, rx::mem::pageSize);
        auto tag = getCacheTag(vmId, sched);

        auto flushedRange = tag.getCache()->flushImages(tag, range);
        flushedRange =
            flushedRange.merge(tag.getCache()->flushImageBuffers(tag, range));

        if (flushedRange) {
          sched.submit();
          sched.wait();
        }

        flushedRange = tag.getCache()->flushBuffers(flushedRange);

        if (flushedRange) {
          unlockReadWrite(vmId, flushedRange.beginAddress(),
                          flushedRange.size());
        } else {
          unlockReadWrite(vmId, range.beginAddress(), range.size());
        }
      }
    }
  });

  commandPipe.device = this;
  commandPipe.ring = {
      .base = std::data(cmdRing),
      .size = std::size(cmdRing),
      .rptr = std::data(cmdRing),
      .wptr = std::data(cmdRing),
  };

  for (auto &pipe : computePipes) {
    pipe.device = this;
  }

  for (int i = 0; i < kGfxPipeCount; ++i) {
    graphicsPipes[i].setDeQueue(
        Ring{
            .base = mainGfxRings[i],
            .size = std::size(mainGfxRings[i]),
            .rptr = mainGfxRings[i],
            .wptr = mainGfxRings[i],
        },
        0);
  }
}

Device::~Device() {
  vkDeviceWaitIdle(vk::context->device);

  if (debugMessenger != VK_NULL_HANDLE) {
    vk::DestroyDebugUtilsMessengerEXT(vk::context->instance, debugMessenger,
                                      vk::context->allocator);
  }

  for (auto fd : dmemFd) {
    if (fd >= 0) {
      ::close(fd);
    }
  }

  for (auto &[pid, info] : processInfo) {
    if (info.vmFd >= 0) {
      ::close(info.vmFd);
    }
  }

  for (auto &cachePage : cachePages) {
    orbis::kfree(cachePage, kCachePageSize);
  }
}

void Device::start() {
  {
    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    vk::context->createSwapchain({
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    });
  }

  for (std::size_t i = 0; i < std::size(dmemFd); ++i) {
    if (dmemFd[i] != -1) {
      continue;
    }

    auto path = std::format("{}/dmem-{}", rx::getShmPath(), i);
    if (!std::filesystem::exists(path)) {
      std::println("Waiting for dmem {}", i);

      while (!std::filesystem::exists(path)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
      }
    }

    dmemFd[i] = ::open(path.c_str(), O_RDWR, S_IRUSR | S_IWUSR);

    if (dmemFd[i] < 0) {
      std::println(stderr, "failed to open dmem {}", path);
      std::abort();
    }
  }

  std::jthread vblankThread([](const std::stop_token &stopToken) {
    orbis::g_context.deviceEventEmitter->emit(
        orbis::kEvFiltDisplay,
        [=](orbis::KNote *note) -> std::optional<orbis::intptr_t> {
          if (DisplayEvent(note->event.ident >> 48) ==
              DisplayEvent::PreVBlankStart) {
            return 0;
          }
          return {};
        });

    auto prevVBlank = std::chrono::steady_clock::now();
    auto period = std::chrono::seconds(1) / 59.94;

    while (!stopToken.stop_requested()) {
      prevVBlank +=
          std::chrono::duration_cast<std::chrono::nanoseconds>(period);
      std::this_thread::sleep_until(prevVBlank);

      orbis::g_context.deviceEventEmitter->emit(
          orbis::kEvFiltDisplay,
          [=](orbis::KNote *note) -> std::optional<orbis::intptr_t> {
            if (DisplayEvent(note->event.ident >> 48) == DisplayEvent::VBlank) {
              return 0;
            }
            return {};
          });
    }
  });

  uint32_t gpIndex = -1;
  GLFWgamepadstate gpState;

  glfwShowWindow(window);

  while (true) {
    glfwPollEvents();

    if (gpIndex > GLFW_JOYSTICK_LAST) {
      for (int i = 0; i <= GLFW_JOYSTICK_LAST; ++i) {
        if (glfwJoystickIsGamepad(i) == GLFW_TRUE) {
          std::print("Gamepad \"{}\" activated", glfwGetGamepadName(i));
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
        kbPadState.leftStickX =
            gpState.axes[GLFW_GAMEPAD_AXIS_LEFT_X] * 127.5f + 127.5f;
        kbPadState.leftStickY =
            gpState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] * 127.5f + 127.5f;
        kbPadState.rightStickX =
            gpState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] * 127.5f + 127.5f;
        kbPadState.rightStickY =
            gpState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] * 127.5f + 127.5f;
        kbPadState.l2 =
            (gpState.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 127.5f;
        kbPadState.r2 =
            (gpState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 127.5f;
        kbPadState.buttons = 0;

        if (kbPadState.l2 == 0xFF) {
          kbPadState.buttons |= kPadBtnL2;
        }

        if (kbPadState.r2 == 0xFF) {
          kbPadState.buttons |= kPadBtnR2;
        }

        static const uint32_t gpmap[GLFW_GAMEPAD_BUTTON_LAST + 1] = {
            [GLFW_GAMEPAD_BUTTON_A] = kPadBtnCross,
            [GLFW_GAMEPAD_BUTTON_B] = kPadBtnCircle,
            [GLFW_GAMEPAD_BUTTON_X] = kPadBtnSquare,
            [GLFW_GAMEPAD_BUTTON_Y] = kPadBtnTriangle,
            [GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = kPadBtnL1,
            [GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = kPadBtnR1,
            [GLFW_GAMEPAD_BUTTON_BACK] = 0,
            [GLFW_GAMEPAD_BUTTON_START] = kPadBtnOptions,
            [GLFW_GAMEPAD_BUTTON_GUIDE] = 0,
            [GLFW_GAMEPAD_BUTTON_LEFT_THUMB] = kPadBtnL3,
            [GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] = kPadBtnR3,
            [GLFW_GAMEPAD_BUTTON_DPAD_UP] = kPadBtnUp,
            [GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = kPadBtnRight,
            [GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = kPadBtnDown,
            [GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = kPadBtnLeft};

        for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
          if (gpState.buttons[i] == GLFW_PRESS) {
            kbPadState.buttons |= gpmap[i];
          }
        }
      }
    } else {
      kbPadState.leftStickX = 0x80;
      kbPadState.leftStickY = 0x80;
      kbPadState.rightStickX = 0x80;
      kbPadState.rightStickY = 0x80;
      kbPadState.buttons = 0;

      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        kbPadState.leftStickX = 0;
      } else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        kbPadState.leftStickX = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        kbPadState.leftStickY = 0;
      } else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        kbPadState.leftStickY = 0xff;
      }

      if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
        kbPadState.rightStickY = 0;
      } else if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        kbPadState.rightStickY = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        kbPadState.rightStickX = 0;
      } else if (glfwGetKey(window, GLFW_KEY_SEMICOLON) == GLFW_PRESS) {
        kbPadState.rightStickX = 0xff;
      }

      if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnUp;
      }
      if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnDown;
      }
      if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnLeft;
      }
      if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnRight;
      }
      if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnSquare;
      }
      if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnCross;
      }
      if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnCircle;
      }
      if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnTriangle;
      }

      if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnL1;
      }
      if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnL2;
        kbPadState.l2 = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnL3;
      }
      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnPs;
      }
      if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnR1;
      }
      if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnR2;
        kbPadState.r2 = 0xff;
      }
      if (glfwGetKey(window, GLFW_KEY_APOSTROPHE) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnR3;
      }

      if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        kbPadState.buttons |= kPadBtnOptions;
      }
    }

    kbPadState.timestamp =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();

    if (glfwWindowShouldClose(window)) {
      rx::shutdown();
      break;
    }

    processPipes();
  }
}

void Device::submitCommand(Ring &ring, std::span<const std::uint32_t> command) {
  if (ring.size < command.size()) {
    std::println(stderr, "too big command: ring size {}, command size {}",
                 ring.size, command.size());
    std::abort();
  }

  std::scoped_lock lock(writeCommandMtx);
  if (ring.wptr + command.size() > ring.base + ring.size) {
    while (ring.wptr != ring.rptr) {
    }

    for (auto it = ring.wptr; it < ring.base + ring.size; ++it) {
      *it = 2 << 30;
    }

    ring.wptr = ring.base;
  }

  std::memcpy(const_cast<std::uint32_t *>(ring.wptr), command.data(),
              command.size_bytes());
  ring.wptr += command.size();
}

void Device::submitGfxCommand(int gfxPipe,
                              std::span<const std::uint32_t> command) {
  auto &ring = graphicsPipes[gfxPipe].deQueues[2];
  submitCommand(ring, command);
}

void Device::mapProcess(std::uint32_t pid, int vmId) {
  auto &process = processInfo[pid];
  process.vmId = vmId;

  auto memory = amdgpu::RemoteMemory{vmId};

  std::string pidVmName = std::format("{}/memory-{}", rx::getShmPath(), pid);
  int memoryFd = ::open(pidVmName.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  process.vmFd = memoryFd;

  if (memoryFd < 0) {
    std::println("failed to open shared memory of process {}", (int)pid);
    std::abort();
  }

  for (auto [startAddress, endAddress, slot] : process.vmTable) {
    auto gpuProt = slot.prot >> 4;
    if (gpuProt == 0) {
      continue;
    }

    auto devOffset = slot.offset + startAddress - slot.baseAddress;
    int mapFd = memoryFd;

    if (slot.memoryType >= 0) {
      mapFd = dmemFd[slot.memoryType];
    }

    auto mmapResult =
        ::mmap(memory.getPointer(startAddress), endAddress - startAddress,
               gpuProt, MAP_FIXED | MAP_SHARED, mapFd, devOffset);

    if (mmapResult == MAP_FAILED) {
      std::println(
          stderr,
          "failed to map process {} memory, address {}-{}, type {:x}, vmId {}",
          (int)pid, memory.getPointer(startAddress),
          memory.getPointer(endAddress), slot.memoryType, vmId);
      std::abort();
    }

    // std::println(stderr,
    //              "map process {} memory, address {}-{}, type {:x}, vmId {}",
    //              (int)pid, memory.getPointer(startAddress),
    //              memory.getPointer(endAddress), slot.memoryType, vmId);
  }
}

void Device::unmapProcess(std::uint32_t pid) {
  auto &process = processInfo[pid];
  auto startAddress = static_cast<std::uint64_t>(process.vmId) << 40;
  auto size = static_cast<std::uint64_t>(1) << 40;

  startAddress += orbis::kMinAddress;
  size -= orbis::kMinAddress;

  rx::mem::reserve(reinterpret_cast<void *>(startAddress), size);

  ::close(process.vmFd);
  process.vmFd = -1;
  process.vmId = -1;
}

void Device::protectMemory(std::uint32_t pid, std::uint64_t address,
                           std::uint64_t size, int prot) {
  auto &process = processInfo[pid];

  auto vmSlotIt = process.vmTable.queryArea(address);
  if (vmSlotIt == process.vmTable.end()) {
    return;
  }

  auto vmSlot = (*vmSlotIt).payload;

  process.vmTable.map(address, address + size,
                      VmMapSlot{
                          .memoryType = vmSlot.memoryType,
                          .prot = static_cast<int>(prot),
                          .offset = vmSlot.offset,
                          .baseAddress = vmSlot.baseAddress,
                      });

  if (process.vmId >= 0) {
    auto memory = amdgpu::RemoteMemory{process.vmId};
    rx::mem::protect(memory.getPointer(address), size, prot >> 4);

    // std::println(stderr, "protect process {} memory, address {}-{}, prot
    // {:x}",
    //              (int)pid, memory.getPointer(address),
    //              memory.getPointer(address + size), prot);
  }
}

void Device::onCommandBuffer(std::uint32_t pid, int cmdHeader,
                             std::uint64_t address, std::uint64_t size) {
  auto &process = processInfo[pid];
  if (process.vmId < 0) {
    return;
  }

  auto memory = RemoteMemory{process.vmId};

  auto op = rx::getBits(cmdHeader, 15, 8);

  if (op == gnm::IT_INDIRECT_BUFFER_CNST) {
    graphicsPipes[0].setCeQueue(Ring::createFromRange(
        process.vmId, memory.getPointer<std::uint32_t>(address),
        size / sizeof(std::uint32_t)));
  } else if (op == gnm::IT_INDIRECT_BUFFER) {
    graphicsPipes[0].setDeQueue(
        Ring::createFromRange(process.vmId,
                              memory.getPointer<std::uint32_t>(address),
                              size / sizeof(std::uint32_t)),
        1);
  } else {
    rx::die("unimplemented command buffer %x", cmdHeader);
  }
}

bool Device::processPipes() {
  bool allProcessed = true;

  commandPipe.processAllRings();

  for (auto &pipe : computePipes) {
    if (!pipe.processAllRings()) {
      allProcessed = false;
    }
  }

  for (auto &pipe : graphicsPipes) {
    if (!pipe.processAllRings()) {
      allProcessed = false;
    }
  }

  return allProcessed;
}

static void
transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
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

  auto layoutToStageAccess =
      [](VkImageLayout layout,
         bool isSrc) -> std::pair<VkPipelineStageFlags, VkAccessFlags> {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    case VK_IMAGE_LAYOUT_GENERAL:
      return {isSrc ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                    : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
              0};

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

  auto [sourceStage, sourceAccess] = layoutToStageAccess(oldLayout, true);
  auto [destinationStage, destinationAccess] =
      layoutToStageAccess(newLayout, false);

  barrier.srcAccessMask = sourceAccess;
  barrier.dstAccessMask = destinationAccess;

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

bool Device::flip(std::uint32_t pid, int bufferIndex, std::uint64_t arg,
                  VkImage swapchainImage, VkImageView swapchainImageView) {
  auto &pipe = graphicsPipes[0];
  auto &scheduler = pipe.scheduler;
  auto &process = processInfo[pid];
  if (process.vmId < 0) {
    return false;
  }

  if (bufferIndex < 0) {
    flipBuffer[process.vmId] = bufferIndex;
    flipArg[process.vmId] = arg;
    flipCount[process.vmId] = flipCount[process.vmId] + 1;
    return false;
  }

  auto &buffer = process.buffers[bufferIndex];
  auto &bufferAttr = process.bufferAttributes[buffer.attrId];

  gnm::DataFormat dfmt;
  gnm::NumericFormat nfmt;
  auto flipType = FlipType::Alt;
  switch (bufferAttr.pixelFormat) {
  case 0x80000000:
    dfmt = gnm::kDataFormat8_8_8_8;
    nfmt = gnm::kNumericFormatSrgb;
    break;

  case 0x80002200:
    dfmt = gnm::kDataFormat8_8_8_8;
    nfmt = gnm::kNumericFormatSrgb;
    flipType = FlipType::Std;
    break;

  case 0x88740000:
  case 0x88060000:
    dfmt = gnm::kDataFormat2_10_10_10;
    nfmt = gnm::kNumericFormatSNorm;
    break;

  case 0x88000000:
    dfmt = gnm::kDataFormat2_10_10_10;
    nfmt = gnm::kNumericFormatSrgb;
    break;

  case 0xc1060000:
    dfmt = gnm::kDataFormat16_16_16_16;
    nfmt = gnm::kNumericFormatFloat;
    break;

  default:
    rx::die("unimplemented color buffer format %x", bufferAttr.pixelFormat);
  }

  // std::printf("displaying buffer %lx\n", buffer.address);

  auto cacheTag = getCacheTag(process.vmId, scheduler);
  auto &sched = cacheTag.getScheduler();

  transitionImageLayout(sched.getCommandBuffer(), swapchainImage,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .levelCount = 1,
                            .layerCount = 1,
                        });

  amdgpu::flip(
      cacheTag, vk::context->swapchainExtent, buffer.address,
      swapchainImageView, {bufferAttr.width, bufferAttr.height}, flipType,
      getDefaultTileModes()[bufferAttr.tilingMode != 0 ? 10 : 8], dfmt, nfmt);

  transitionImageLayout(sched.getCommandBuffer(), swapchainImage,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .levelCount = 1,
                            .layerCount = 1,
                        });

  sched.submit();

  auto submitCompleteTask = scheduler.createExternalSubmit();

  {
    VkSemaphoreSubmitInfo waitSemSubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk::context->presentCompleteSemaphore,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = scheduler.getSemaphoreHandle(),
            .value = submitCompleteTask - 1,
            .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        },
    };

    VkSemaphoreSubmitInfo signalSemSubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk::context->renderCompleteSemaphore,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = scheduler.getSemaphoreHandle(),
            .value = submitCompleteTask,
            .stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        },
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 2,
        .pWaitSemaphoreInfos = waitSemSubmitInfos,
        .signalSemaphoreInfoCount = 2,
        .pSignalSemaphoreInfos = signalSemSubmitInfos,
    };

    vkQueueSubmit2(vk::context->presentQueue, 1, &submitInfo, VK_NULL_HANDLE);
  }

  scheduler.then([=, this, cacheTag = std::move(cacheTag)] {
    flipBuffer[process.vmId] = bufferIndex;
    flipArg[process.vmId] = arg;
    flipCount[process.vmId] = flipCount[process.vmId] + 1;

    auto mem = RemoteMemory{process.vmId};
    auto bufferInUse =
        mem.getPointer<std::uint64_t>(bufferInUseAddress[process.vmId]);
    if (bufferInUse != nullptr) {
      bufferInUse[bufferIndex] = 0;
    }
  });

  return true;
}

void Device::flip(std::uint32_t pid, int bufferIndex, std::uint64_t arg) {
  auto recreateSwapchain = [this] {
    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);
    vk::context->recreateSwapchain({
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    });
  };

  if (!isImageAcquired) {
    while (true) {
      auto acquireNextImageResult = vkAcquireNextImageKHR(
          vk::context->device, vk::context->swapchain, UINT64_MAX,
          vk::context->presentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);
      if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        continue;
      }

      if (acquireNextImageResult != VK_SUBOPTIMAL_KHR) {
        VK_VERIFY(acquireNextImageResult);
      }
      break;
    }
  }

  bool flipComplete =
      flip(pid, bufferIndex, arg, vk::context->swapchainImages[imageIndex],
           vk::context->swapchainImageViews[imageIndex]);

  orbis::g_context.deviceEventEmitter->emit(
      orbis::kEvFiltDisplay,
      [=](orbis::KNote *note) -> std::optional<orbis::intptr_t> {
        if (DisplayEvent(note->event.ident >> 48) == DisplayEvent::Flip) {
          return arg;
        }
        return {};
      });

  if (!flipComplete) {
    isImageAcquired = true;
    return;
  }

  isImageAcquired = false;

  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &vk::context->renderCompleteSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &vk::context->swapchain,
      .pImageIndices = &imageIndex,
  };

  auto vkQueuePresentResult =
      vkQueuePresentKHR(vk::context->presentQueue, &presentInfo);

  if (vkQueuePresentResult == VK_ERROR_OUT_OF_DATE_KHR ||
      vkQueuePresentResult == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain();
  } else {
    VK_VERIFY(vkQueuePresentResult);
  }
}

void Device::waitForIdle() {
  while (true) {
    bool allProcessed = true;
    for (auto &queue : graphicsPipes[0].deQueues) {
      if (queue.wptr != queue.rptr) {
        allProcessed = false;
      }
    }

    {
      auto &queue = graphicsPipes[0].ceQueue;
      if (queue.wptr != queue.rptr) {
        allProcessed = false;
      }
    }

    if (allProcessed) {
      break;
    }
  }
}

void Device::mapMemory(std::uint32_t pid, std::uint64_t address,
                       std::uint64_t size, int memoryType, int dmemIndex,
                       int prot, std::int64_t offset) {
  auto &process = processInfo[pid];

  process.vmTable.map(address, address + size,
                      VmMapSlot{
                          .memoryType = memoryType >= 0 ? dmemIndex : -1,
                          .prot = prot,
                          .offset = offset,
                          .baseAddress = address,
                      });

  if (process.vmId < 0) {
    return;
  }

  auto memory = amdgpu::RemoteMemory{process.vmId};

  int mapFd = process.vmFd;

  if (memoryType >= 0) {
    mapFd = dmemFd[dmemIndex];
  }

  auto mmapResult = ::mmap(memory.getPointer(address), size, prot >> 4,
                           MAP_FIXED | MAP_SHARED, mapFd, offset);

  if (mmapResult == MAP_FAILED) {
    perror("::mmap");

    rx::mem::printStats();
    rx::die("failed to map process %u memory, address %p-%p, type %x, offset "
            "%lx, prot %x",
            (int)pid, memory.getPointer(address),
            memory.getPointer(address + size), memoryType, offset, prot);
  }

  // std::println(stderr, "map memory of process {}, address {}-{}, prot {:x}",
  //              (int)pid, memory.getPointer(address),
  //              memory.getPointer(address + size), prot);
}

void Device::unmapMemory(std::uint32_t pid, std::uint64_t address,
                         std::uint64_t size) {
  // TODO
  protectMemory(pid, address, size, 0);
}

static void notifyPageChanges(Device *device, int vmId, std::uint32_t firstPage,
                              std::uint32_t pageCount) {
  std::uint64_t command =
      (static_cast<std::uint64_t>(pageCount - 1) << 32) | firstPage;

  while (true) {
    for (std::size_t i = 0; i < std::size(device->cpuCacheCommands); ++i) {
      std::uint64_t expCommand = 0;
      if (device->cpuCacheCommands[vmId][i].compare_exchange_strong(
              expCommand, command, std::memory_order::release,
              std::memory_order::relaxed)) {
        device->cpuCacheCommandsIdle[vmId].fetch_add(
            1, std::memory_order::release);
        device->cpuCacheCommandsIdle[vmId].notify_one();

        while (device->cpuCacheCommands[vmId][i].load(
                   std::memory_order::acquire) != 0) {
        }
        return;
      }
    }
  }
}

static void modifyWatchFlags(Device *device, int vmId, std::uint64_t address,
                             std::uint64_t size, std::uint8_t addFlags,
                             std::uint8_t removeFlags) {
  auto firstPage = address / rx::mem::pageSize;
  auto lastPage = (address + size + rx::mem::pageSize - 1) / rx::mem::pageSize;
  bool hasChanges = false;
  for (auto page = firstPage; page < lastPage; ++page) {
    auto prevValue =
        device->cachePages[vmId][page].load(std::memory_order::relaxed);
    auto newValue = (prevValue & ~removeFlags) | addFlags;

    if (newValue == prevValue) {
      continue;
    }

    while (!device->cachePages[vmId][page].compare_exchange_weak(
        prevValue, newValue, std::memory_order::relaxed)) {
      newValue = (prevValue & ~removeFlags) | addFlags;
    }

    if (newValue != prevValue) {
      hasChanges = true;
    }
  }

  if (hasChanges) {
    notifyPageChanges(device, vmId, firstPage, lastPage - firstPage);
  }
}

void Device::watchWrites(int vmId, std::uint64_t address, std::uint64_t size) {
  modifyWatchFlags(this, vmId, address, size, kPageWriteWatch,
                   kPageInvalidated);
}
void Device::lockReadWrite(int vmId, std::uint64_t address, std::uint64_t size,
                           bool isLazy) {
  modifyWatchFlags(this, vmId, address, size,
                   kPageReadWriteLock | (isLazy ? kPageLazyLock : 0),
                   kPageInvalidated);
}
void Device::unlockReadWrite(int vmId, std::uint64_t address,
                             std::uint64_t size) {
  modifyWatchFlags(this, vmId, address, size, kPageWriteWatch,
                   kPageReadWriteLock | kPageLazyLock);
}
