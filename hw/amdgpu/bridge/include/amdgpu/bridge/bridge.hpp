#pragma once

#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace amdgpu::bridge {
enum class CommandId : std::uint32_t {
  Nop,
  SetUpSharedMemory,
  ProtectMemory,
  CommandBuffer,
  Flip,
  DoFlip,
  SetBuffer
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
  std::uint32_t bufferIndex;
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

class BridgePusher {
  BridgeHeader *buffer = nullptr;

public:
  BridgePusher() = default;
  BridgePusher(BridgeHeader *buffer) : buffer(buffer) {}

  void setVm(std::uint64_t address, std::uint64_t size, const char *name) {
    buffer->vmAddress = address;
    buffer->vmSize = size;
    std::strncpy(buffer->vmName, name, sizeof(buffer->vmName));
    buffer->flags |= static_cast<std::uint64_t>(BridgeFlags::VmConfigured);
  }

  void sendMemoryProtect(std::uint64_t address, std::uint64_t size,
                         std::uint32_t prot) {
    sendCommand(CommandId::ProtectMemory, {address, size, prot});
  }

  void sendCommandBuffer(std::uint64_t queue, std::uint64_t address,
                         std::uint64_t size) {
    sendCommand(CommandId::CommandBuffer, {queue, address, size});
  }

  void sendSetBuffer(std::uint32_t bufferIndex, std::uint64_t address,
                     std::uint32_t width, std::uint32_t height,
                     std::uint32_t pitch, std::uint32_t pixelFormat,
                     std::uint32_t tilingMode) {
    sendCommand(CommandId::SetBuffer,
                {static_cast<std::uint64_t>(bufferIndex) << 32 | tilingMode,
                 address, static_cast<std::uint64_t>(width) << 32 | height,
                 static_cast<std::uint64_t>(pitch) << 32 | pixelFormat});
  }

  void sendFlip(std::uint32_t bufferIndex, std::uint64_t arg) {
    sendCommand(CommandId::Flip, {bufferIndex, arg});
  }

  void sendDoFlip() { sendCommand(CommandId::DoFlip, {}); }

  void wait() {
    while (buffer->pull != buffer->push)
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

    buffer->commands[pos++] = makeCommandHeader(CommandId::Flip, cmdSize);
    for (auto arg : args) {
      buffer->commands[pos++] = arg;
    }
    buffer->push = pos;
  }

  std::uint64_t getPushPosition(std::uint64_t cmdSize) {
    std::uint64_t position = buffer->push;

    if (position + cmdSize > buffer->size) {
      if (position < buffer->size) {
        buffer->commands[position] =
            static_cast<std::uint64_t>(CommandId::Nop) |
            ((buffer->size - position - 1) << 32);
      }

      position = 0;
      waitPuller(cmdSize);
    }

    return position;
  }
  void waitPuller(std::uint64_t pullValue) {
    while (buffer->pull < pullValue) {
      ;
    }
  }
};

class BridgePuller {
  BridgeHeader *buffer = nullptr;

public:
  BridgePuller() = default;
  BridgePuller(BridgeHeader *buffer) : buffer(buffer) {}

  std::size_t pullCommands(Command *commands, std::size_t maxCount) {
    std::size_t processed = 0;

    while (processed < maxCount) {
      if (buffer->pull == buffer->push) {
        break;
      }

      auto pos = buffer->pull;
      auto cmd = buffer->commands[pos];
      CommandId cmdId = static_cast<CommandId>(cmd);
      std::uint32_t argsCount = cmd >> 32;

      if (cmdId != CommandId::Nop) {
        commands[processed++] =
            unpackCommand(cmdId, buffer->commands + pos + 1, argsCount);
      }

      auto newPull = pos + argsCount + 1;

      if (newPull >= buffer->size) {
        newPull = 0;
      }

      buffer->pull = newPull;
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
    case CommandId::SetUpSharedMemory:
    case CommandId::DoFlip:
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

    case CommandId::SetBuffer:
      result.buffer.bufferIndex = static_cast<std::uint32_t>(args[0] >> 32);
      result.buffer.address = args[1];
      result.buffer.width = static_cast<std::uint32_t>(args[2] >> 32);
      result.buffer.height = static_cast<std::uint32_t>(args[2]);
      result.buffer.pitch = static_cast<std::uint32_t>(args[3] >> 32);
      result.buffer.pixelFormat = static_cast<std::uint32_t>(args[3]);
      result.buffer.tilingMode = static_cast<std::uint32_t>(args[0]);
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
