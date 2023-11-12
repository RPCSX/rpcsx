#pragma once

#include "orbis/utils/SharedMutex.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <orbis/utils/SharedCV.hpp>

namespace amdgpu::bridge {
struct PadState {
  std::uint64_t timestamp;
  std::uint32_t unk;
  std::uint32_t buttons;
  std::uint8_t leftStickX;
  std::uint8_t leftStickY;
  std::uint8_t rightStickX;
  std::uint8_t rightStickY;
  std::uint8_t l2;
  std::uint8_t r2;
};

enum {
  kPadBtnL3 = 1 << 1,
  kPadBtnR3 = 1 << 2,
  kPadBtnOptions = 1 << 3,
  kPadBtnUp = 1 << 4,
  kPadBtnRight = 1 << 5,
  kPadBtnDown = 1 << 6,
  kPadBtnLeft = 1 << 7,
  kPadBtnL2 = 1 << 8,
  kPadBtnR2 = 1 << 9,
  kPadBtnL1 = 1 << 10,
  kPadBtnR1 = 1 << 11,
  kPadBtnTriangle = 1 << 12,
  kPadBtnCircle = 1 << 13,
  kPadBtnCross = 1 << 14,
  kPadBtnSquare = 1 << 15,
  kPadBtnTouchPad = 1 << 20,
  kPadBtnIntercepted = 1 << 31,
};

enum class CommandId : std::uint32_t {
  Nop,
  ProtectMemory,
  CommandBuffer,
  Flip
};

struct CmdMemoryProt {
  std::uint64_t address;
  std::uint64_t size;
  std::uint32_t prot;
  std::uint32_t pid;
};

struct CmdCommandBuffer {
  std::uint64_t queue;
  std::uint64_t address;
  std::uint32_t size;
  std::uint32_t pid;
};

struct CmdBuffer {
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t pitch;
  std::uint64_t address;
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode;
};

struct CmdFlip {
  std::uint32_t pid;
  std::uint32_t bufferIndex;
  std::uint64_t arg;
};

enum {
  kPageWriteWatch = 1 << 0,
  kPageReadWriteLock = 1 << 1,
  kPageInvalidated = 1 << 2,
  kPageLazyLock = 1 << 3
};

static constexpr auto kHostPageSize = 0x1000;

struct BridgeHeader {
  std::uint64_t size;
  std::uint64_t info;
  std::uint32_t pullerPid;
  std::uint32_t pusherPid;
  volatile std::uint64_t flags;
  std::uint64_t vmAddress;
  std::uint64_t vmSize;
  char vmName[32];
  PadState kbPadState;
  volatile std::uint32_t flipBuffer;
  volatile std::uint64_t flipArg;
  volatile std::uint64_t flipCount;
  volatile std::uint64_t bufferInUseAddress;
  std::uint32_t memoryAreaCount;
  std::uint32_t commandBufferCount;
  std::uint32_t bufferCount;
  CmdMemoryProt memoryAreas[128];
  CmdCommandBuffer commandBuffers[32];
  CmdBuffer buffers[10];
  // orbis::shared_mutex cacheCommandMtx;
  // orbis::shared_cv cacheCommandCv;
  std::atomic<std::uint64_t> cacheCommands[4];
  std::atomic<std::uint32_t> gpuCacheCommand;
  std::atomic<std::uint8_t> cachePages[0x100'0000'0000 / kHostPageSize];

  volatile std::uint64_t pull;
  volatile std::uint64_t push;
  std::uint64_t commands[];
};

struct Command {
  CommandId id;

  union {
    CmdMemoryProt memoryProt;
    CmdCommandBuffer commandBuffer;
    CmdBuffer buffer;
    CmdFlip flip;
  };
};

enum class BridgeFlags {
  VmConfigured = 1 << 0,
  PushLock = 1 << 1,
  PullLock = 1 << 2,
};

struct BridgePusher {
  BridgeHeader *header = nullptr;

  BridgePusher() = default;
  BridgePusher(BridgeHeader *header) : header(header) {}

  void setVm(std::uint64_t address, std::uint64_t size, const char *name) {
    header->vmAddress = address;
    header->vmSize = size;
    std::strncpy(header->vmName, name, sizeof(header->vmName));
    header->flags =
        header->flags | static_cast<std::uint64_t>(BridgeFlags::VmConfigured);
  }

  void sendMemoryProtect(std::uint32_t pid, std::uint64_t address,
                         std::uint64_t size, std::uint32_t prot) {
    if (pid == 50001) {
      sendCommand(CommandId::ProtectMemory, {pid, address, size, prot});
    }
  }

  void sendCommandBuffer(std::uint32_t pid, std::uint64_t queue,
                         std::uint64_t address, std::uint64_t size) {
    if (pid == 50001) {
      sendCommand(CommandId::CommandBuffer, {pid, queue, address, size});
    }
  }

  void sendFlip(std::uint32_t pid, std::uint32_t bufferIndex,
                std::uint64_t arg) {
    if (pid == 50001) {
      sendCommand(CommandId::Flip, {pid, bufferIndex, arg});
    }
  }

  void wait() {
    while (header->pull != header->push)
      ;
  }

private:
  static std::uint64_t makeCommandHeader(CommandId id, std::size_t cmdSize) {
    return static_cast<std::uint64_t>(id) |
           (static_cast<std::uint64_t>(cmdSize - 1) << 32);
  }

  void sendCommand(CommandId id, std::initializer_list<std::uint64_t> args) {
    std::size_t cmdSize = args.size() + 1;
    std::uint64_t pos = getPushPosition(cmdSize);

    header->commands[pos++] = makeCommandHeader(id, cmdSize);
    for (auto arg : args) {
      header->commands[pos++] = arg;
    }
    header->push = pos;
  }

  std::uint64_t getPushPosition(std::uint64_t cmdSize) {
    std::uint64_t position = header->push;

    if (position + cmdSize > header->size) {
      if (position < header->size) {
        header->commands[position] =
            static_cast<std::uint64_t>(CommandId::Nop) |
            ((header->size - position - 1) << 32);
      }

      position = 0;
      waitPuller(cmdSize);
    }

    return position;
  }
  void waitPuller(std::uint64_t pullValue) {
    while (header->pull < pullValue) {
      ;
    }
  }
};

struct BridgePuller {
  BridgeHeader *header = nullptr;

  BridgePuller() = default;
  BridgePuller(BridgeHeader *header) : header(header) {}

  std::size_t pullCommands(Command *commands, std::size_t maxCount) {
    std::size_t processed = 0;

    while (processed < maxCount) {
      if (header->pull == header->push) {
        break;
      }

      auto pos = header->pull;
      auto cmd = header->commands[pos];
      CommandId cmdId = static_cast<CommandId>(cmd);
      std::uint32_t argsCount = cmd >> 32;

      if (cmdId != CommandId::Nop) {
        commands[processed++] =
            unpackCommand(cmdId, header->commands + pos + 1, argsCount);
      }

      auto newPull = pos + argsCount + 1;

      if (newPull >= header->size) {
        newPull = 0;
      }

      header->pull = newPull;
    }

    return processed;
  }

private:
  Command unpackCommand(CommandId command, const std::uint64_t *args,
                        std::uint32_t argsCount) {
    Command result;
    result.id = command;

    switch (command) {
    case CommandId::Nop:
      return result;

    case CommandId::ProtectMemory:
      result.memoryProt.pid = args[0];
      result.memoryProt.address = args[1];
      result.memoryProt.size = args[2];
      result.memoryProt.prot = args[3];
      return result;

    case CommandId::CommandBuffer:
      result.commandBuffer.pid = args[0];
      result.commandBuffer.queue = args[1];
      result.commandBuffer.address = args[2];
      result.commandBuffer.size = args[3];
      return result;

    case CommandId::Flip:
      result.flip.pid = args[0];
      result.flip.bufferIndex = args[1];
      result.flip.arg = args[2];
      return result;
    }

    __builtin_trap();
  }
};

BridgeHeader *createShmCommandBuffer(const char *name);
BridgeHeader *openShmCommandBuffer(const char *name);
void destroyShmCommandBuffer(BridgeHeader *buffer);
void unlinkShm(const char *name);
} // namespace amdgpu::bridge
