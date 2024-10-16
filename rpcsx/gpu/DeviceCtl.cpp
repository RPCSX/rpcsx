#include "DeviceCtl.hpp"
#include "Device.hpp"
#include "gnm/pm4.hpp"
#include "rx/bits.hpp"
#include "rx/die.hpp"
#include "shader/dialect.hpp"
#include <cstdio>
#include <print>
#include <vector>

using namespace amdgpu;

DeviceCtl::DeviceCtl() noexcept = default;
DeviceCtl::DeviceCtl(orbis::Ref<orbis::RcBase> device) noexcept
    : mDevice(device.rawStaticCast<Device>()) {}
DeviceCtl::DeviceCtl(DeviceCtl &&) noexcept = default;
DeviceCtl::DeviceCtl(const DeviceCtl &) = default;
DeviceCtl &DeviceCtl::operator=(DeviceCtl &&) noexcept = default;
DeviceCtl &DeviceCtl::operator=(const DeviceCtl &) = default;

DeviceCtl::~DeviceCtl() = default;

DeviceCtl DeviceCtl::createDevice() {
  DeviceCtl result;
  result.mDevice = orbis::knew<Device>();
  return result;
}

DeviceContext &DeviceCtl::getContext() { return *mDevice.get(); }
orbis::Ref<orbis::RcBase> DeviceCtl::getOpaque() { return mDevice; }

void DeviceCtl::submitGfxCommand(int gfxPipe, int vmId,
                                 std::span<const std::uint32_t> command) {
  auto op = rx::getBits(command[0], 15, 8);
  auto type = rx::getBits(command[0], 31, 30);
  auto len = rx::getBits(command[0], 29, 16) + 2;

  if ((op != gnm::IT_INDIRECT_BUFFER && op != gnm::IT_INDIRECT_BUFFER_CNST) ||
      type != 3 || len != 4 || command.size() != len) {
    std::println(stderr, "unexpected gfx command for main ring: {}, {}, {}", op,
                 type, len);
    rx::die("");
  }

  std::vector<std::uint32_t> patchedCommand{command.data(),
                                            command.data() + command.size()};
  patchedCommand[3] &= ~(~0 << 24);
  patchedCommand[3] |= vmId << 24;

  mDevice->submitGfxCommand(gfxPipe, patchedCommand);
}

void DeviceCtl::submitSwitchBuffer(int gfxPipe) {
  mDevice->submitGfxCommand(gfxPipe, createPm4Packet(gnm::IT_SWITCH_BUFFER, 0));
}
void DeviceCtl::submitFlip(int gfxPipe, std::uint32_t pid, int bufferIndex,
                           std::uint64_t flipArg) {
  mDevice->submitGfxCommand(gfxPipe, createPm4Packet(IT_FLIP, bufferIndex,
                                                     flipArg & 0xffff'ffff,
                                                     flipArg >> 32, pid));
}

orbis::ErrorCode DeviceCtl::submitFlipOnEop(int gfxPipe, std::uint32_t pid,
                                            int bufferIndex,
                                            std::uint64_t flipArg) {
  int index;
  auto &pipe = mDevice->graphicsPipes[gfxPipe];
  {
    std::lock_guard lock(pipe.eopFlipMtx);
    if (pipe.eopFlipRequestCount >= GraphicsPipe::kEopFlipRequestMax) {
      return orbis::ErrorCode::AGAIN;
    }

    index = pipe.eopFlipRequestCount++;

    pipe.eopFlipRequests[index] = {
        .pid = pid,
        .bufferIndex = bufferIndex,
        .arg = flipArg,
    };
  }

  return {};
}

void DeviceCtl::submitMapMemory(int gfxPipe, std::uint32_t pid,
                                std::uint64_t address, std::uint64_t size,
                                int memoryType, int dmemIndex, int prot,
                                std::int64_t offset) {
  mDevice->submitGfxCommand(
      gfxPipe,
      createPm4Packet(IT_MAP_MEMORY, pid, address & 0xffff'ffff, address >> 32,
                      size & 0xffff'ffff, size >> 32, memoryType, dmemIndex,
                      prot, offset & 0xffff'ffff, offset >> 32));
}
void DeviceCtl::submitUnmapMemory(int gfxPipe, std::uint32_t pid,
                                  std::uint64_t address, std::uint64_t size) {
  mDevice->submitGfxCommand(
      gfxPipe, createPm4Packet(IT_UNMAP_MEMORY, pid, address & 0xffff'ffff,
                               address >> 32, size & 0xffff'ffff, size >> 32));
}

void DeviceCtl::submitMapProcess(int gfxPipe, std::uint32_t pid, int vmId) {
  mDevice->submitGfxCommand(gfxPipe,
                            createPm4Packet(gnm::IT_MAP_PROCESS, pid, vmId));
}

void DeviceCtl::submitUnmapProcess(int gfxPipe, std::uint32_t pid) {
  mDevice->submitGfxCommand(gfxPipe, createPm4Packet(IT_UNMAP_PROCESS, pid));
}

void DeviceCtl::submitProtectMemory(int gfxPipe, std::uint32_t pid,
                                    std::uint64_t address, std::uint64_t size,
                                    int prot) {
  mDevice->submitGfxCommand(
      gfxPipe,
      createPm4Packet(IT_PROTECT_MEMORY, pid, address & 0xffff'ffff,
                      address >> 32, size & 0xffff'ffff, size >> 32, prot));
}

void DeviceCtl::registerBuffer(std::uint32_t pid, Buffer buffer) {
  // FIXME: submit command
  auto &process = mDevice->processInfo[pid];

  if (buffer.attrId >= 10 || buffer.index >= 10) {
    rx::die("out of buffers %u, %u", buffer.attrId, buffer.index);
  }

  process.buffers[buffer.index] = buffer;
}

void DeviceCtl::registerBufferAttribute(std::uint32_t pid,
                                        BufferAttribute attr) {
  // FIXME: submit command
  auto &process = mDevice->processInfo[pid];
  if (attr.attrId >= 10) {
    rx::die("out of buffer attributes %u", attr.attrId);
  }

  process.bufferAttributes[attr.attrId] = attr;
}

void DeviceCtl::mapComputeQueue(int vmId, std::uint32_t meId,
                                std::uint32_t pipeId, std::uint32_t queueId,
                                std::uint32_t vqueueId,
                                orbis::uint64_t ringBaseAddress,
                                orbis::uint64_t readPtrAddress,
                                orbis::uint64_t doorbell,
                                orbis::uint64_t ringSize) {
  if (meId != 1) {
    rx::die("unexpected ME %d", meId);
  }

  auto &pipe = mDevice->computePipes[pipeId];
  auto lock = pipe.lockQueue(queueId);
  auto memory = RemoteMemory{vmId};
  auto base = memory.getPointer<std::uint32_t>(ringBaseAddress);
  pipe.mapQueue(queueId,
                Ring{
                    .vmId = vmId,
                    .indirectLevel = 0,
                    .doorbell = memory.getPointer<std::uint32_t>(doorbell),
                    .base = base,
                    .size = ringSize,
                    .rptr = base,
                    .wptr = base,
                    .rptrReportLocation =
                        memory.getPointer<std::uint32_t>(readPtrAddress),
                },
                lock);

  auto config = std::bit_cast<amdgpu::Registers::ComputeConfig *>(doorbell);
  config->state = 1;
}

void DeviceCtl::submitComputeQueue(std::uint32_t meId, std::uint32_t pipeId,
                                   std::uint32_t queueId,
                                   std::uint64_t offset) {
  if (meId != 1) {
    rx::die("unexpected ME %d", meId);
  }

  auto &pipe = mDevice->computePipes[pipeId];
  pipe.submit(queueId, offset);
}

void DeviceCtl::start() { mDevice->start(); }
void DeviceCtl::waitForIdle() { mDevice->waitForIdle(); }
