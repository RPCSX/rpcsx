#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <orbis/utils/SharedMutex.hpp>

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
  kPadBtnPs = 1 << 16,
  kPadBtnTouchPad = 1 << 20,
  kPadBtnIntercepted = 1 << 31,
};

enum class CommandId : std::uint32_t {
  Nop,
  ProtectMemory,
  CommandBuffer,
  Flip,
  MapMemory,
  MapProcess,
  UnmapProcess,
  RegisterBuffer,
  RegisterBufferAttribute,
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

struct CmdBufferAttribute {
  std::uint32_t pid;
  std::uint8_t attrId;
  std::uint8_t submit;
  std::uint64_t canary;
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode;
  std::uint32_t pitch;
  std::uint32_t width;
  std::uint32_t height;
};

struct CmdBuffer {
  std::uint64_t canary;
  std::uint32_t index;
  std::uint32_t attrId;
  std::uint64_t address;
  std::uint64_t address2;
  std::uint32_t pid;
};

struct CmdFlip {
  std::uint32_t pid;
  std::uint32_t bufferIndex;
  std::uint64_t arg;
};

struct CmdMapMemory {
  std::int64_t offset;
  std::uint64_t address;
  std::uint64_t size;
  std::uint32_t prot;
  std::uint32_t pid;
  std::int32_t memoryType;
  std::uint32_t dmemIndex;
};

struct CmdMapProcess {
  std::uint64_t pid;
  int vmId;
};

struct CmdUnmapProcess {
  std::uint64_t pid;
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
  std::atomic<std::uint64_t> lock;
  volatile std::uint64_t flags;
  std::uint64_t vmAddress;
  std::uint64_t vmSize;
  char vmName[32];
  PadState kbPadState;
  volatile std::uint32_t flipBuffer[6];
  volatile std::uint64_t flipArg[6];
  volatile std::uint64_t flipCount[6];
  volatile std::uint64_t bufferInUseAddress[6];
  std::uint32_t commandBufferCount;
  std::uint32_t bufferCount;
  CmdCommandBuffer commandBuffers[32];
  // CmdBuffer buffers[10];
  // orbis::shared_mutex cacheCommandMtx;
  // orbis::shared_cv cacheCommandCv;
  std::atomic<std::uint64_t> cacheCommands[6][4];
  std::atomic<std::uint32_t> gpuCacheCommand[6];
  std::atomic<std::uint8_t> cachePages[6][0x100'0000'0000 / kHostPageSize];

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
    CmdBufferAttribute bufferAttribute;
    CmdFlip flip;
    CmdMapMemory mapMemory;
    CmdMapProcess mapProcess;
    CmdUnmapProcess unmapProcess;
  };
};

enum class BridgeFlags {
  VmConfigured = 1 << 0,
  PushLock = 1 << 1,
  PullLock = 1 << 2,
};

struct BridgePusher {
  BridgeHeader *header = nullptr;

  void setVm(std::uint64_t address, std::uint64_t size, const char *name) {
    header->vmAddress = address;
    header->vmSize = size;
    std::strncpy(header->vmName, name, sizeof(header->vmName));
    header->flags =
        header->flags | static_cast<std::uint64_t>(BridgeFlags::VmConfigured);
  }

  void sendMemoryProtect(std::uint32_t pid, std::uint64_t address,
                         std::uint64_t size, std::uint32_t prot) {
    sendCommand(CommandId::ProtectMemory, {pid, address, size, prot});
  }

  void sendMapMemory(std::uint32_t pid, std::uint32_t memoryType,
                     std::uint32_t dmemIndex, std::uint64_t address,
                     std::uint64_t size, std::uint32_t prot,
                     std::uint64_t offset) {
    sendCommand(CommandId::MapMemory,
                {pid, memoryType, dmemIndex, address, size, prot, offset});
  }

  void sendRegisterBuffer(std::uint32_t pid, std::uint64_t canary,
                          std::uint32_t index, std::uint32_t attrId,
                          std::uint64_t address, std::uint64_t address2) {
    sendCommand(CommandId::RegisterBuffer,
                {pid, canary, index, attrId, address, address2});
  }
  void sendRegisterBufferAttribute(std::uint32_t pid, std::uint8_t attrId,
                                   std::uint8_t submit, std::uint64_t canary,
                                   std::uint32_t pixelFormat,
                                   std::uint32_t tilingMode,
                                   std::uint32_t pitch, std::uint32_t width,
                                   std::uint32_t height) {
    sendCommand(CommandId::RegisterBufferAttribute,
                {pid, attrId, submit, canary, pixelFormat, tilingMode, pitch,
                 width, height});
  }

  void sendCommandBuffer(std::uint32_t pid, std::uint64_t queue,
                         std::uint64_t address, std::uint64_t size) {
    sendCommand(CommandId::CommandBuffer, {pid, queue, address, size});
  }

  void sendFlip(std::uint32_t pid, std::uint32_t bufferIndex,
                std::uint64_t arg) {
    sendCommand(CommandId::Flip, {pid, bufferIndex, arg});
  }

  void sendMapProcess(std::uint32_t pid, unsigned vmId) {
    sendCommand(CommandId::MapProcess, {pid, vmId});
  }
  void sendUnmapProcess(std::uint32_t pid) {
    sendCommand(CommandId::UnmapProcess, {pid});
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
    std::uint64_t exp = 0;
    while (!header->lock.compare_exchange_weak(
        exp, 1, std::memory_order::acquire, std::memory_order::relaxed)) {
      exp = 0;
    }

    std::size_t cmdSize = args.size() + 1;
    std::uint64_t pos = getPushPosition(cmdSize);

    header->commands[pos++] = makeCommandHeader(id, cmdSize);
    for (auto arg : args) {
      header->commands[pos++] = arg;
    }
    header->push = pos;
    header->lock.store(0, std::memory_order::release);
  }

  std::uint64_t getPushPosition(std::uint64_t cmdSize) {
    std::uint64_t position = header->push;

    if (position + cmdSize > header->size) {
      if (position < header->size) {
        header->commands[position] =
            static_cast<std::uint64_t>(CommandId::Nop) |
            ((header->size - position + cmdSize) << 32);
      }

      position = 0;
      header->push = position;
      waitPuller(position);
    }

    return position;
  }
  void waitPuller(std::uint64_t pullValue) {
    while (header->pull != pullValue) {
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

    case CommandId::MapMemory:
      result.mapMemory.pid = args[0];
      result.mapMemory.memoryType = args[1];
      result.mapMemory.dmemIndex = args[2];
      result.mapMemory.address = args[3];
      result.mapMemory.size = args[4];
      result.mapMemory.prot = args[5];
      result.mapMemory.offset = args[6];
      return result;

    case CommandId::MapProcess:
      result.mapProcess.pid = args[0];
      result.mapProcess.vmId = args[1];
      return result;

    case CommandId::UnmapProcess:
      result.unmapProcess.pid = args[0];
      return result;

    case CommandId::RegisterBufferAttribute:
      result.bufferAttribute.pid = args[0];
      result.bufferAttribute.attrId = args[1];
      result.bufferAttribute.submit = args[2];
      result.bufferAttribute.canary = args[3];
      result.bufferAttribute.pixelFormat = args[4];
      result.bufferAttribute.tilingMode = args[5];
      result.bufferAttribute.pitch = args[6];
      result.bufferAttribute.width = args[7];
      result.bufferAttribute.height = args[8];
      return result;

    case CommandId::RegisterBuffer:
      result.buffer.pid = args[0];
      result.buffer.canary = args[1];
      result.buffer.index = args[2];
      result.buffer.attrId = args[3];
      result.buffer.address = args[4];
      result.buffer.address2 = args[5];
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
