#pragma once

#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace amdgpu::bridge {
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
};

struct CmdCommandBuffer {
  std::uint64_t queue;
  std::uint64_t address;
  std::uint64_t size;
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
  std::uint32_t bufferIndex;
  std::uint64_t arg;
};

struct BridgeHeader {
  std::uint64_t size;
  std::uint64_t info;
  std::uint32_t pullerPid;
  std::uint32_t pusherPid;
  volatile std::uint64_t flags;
  std::uint64_t vmAddress;
  std::uint64_t vmSize;
  char vmName[32];
  volatile std::uint32_t flipBuffer;
  volatile std::uint64_t flipArg;
  volatile std::uint64_t flipCount;
  std::uint32_t memoryAreaCount;
  std::uint32_t commandBufferCount;
  std::uint32_t bufferCount;
  CmdMemoryProt memoryAreas[128];
  CmdCommandBuffer commandBuffers[32];
  CmdBuffer buffers[8];

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

  void sendMemoryProtect(std::uint64_t address, std::uint64_t size,
                         std::uint32_t prot) {
    sendCommand(CommandId::ProtectMemory, {address, size, prot});
  }

  void sendCommandBuffer(std::uint64_t queue, std::uint64_t address,
                         std::uint64_t size) {
    sendCommand(CommandId::CommandBuffer, {queue, address, size});
  }

  void sendFlip(std::uint32_t bufferIndex, std::uint64_t arg) {
    sendCommand(CommandId::Flip, {bufferIndex, arg});
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
      result.memoryProt.address = args[0];
      result.memoryProt.size = args[1];
      result.memoryProt.prot = args[2];
      return result;

    case CommandId::CommandBuffer:
      result.commandBuffer.queue = args[0];
      result.commandBuffer.address = args[1];
      result.commandBuffer.size = args[2];
      return result;

    case CommandId::Flip:
      result.flip.bufferIndex = args[0];
      result.flip.arg = args[1];
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
