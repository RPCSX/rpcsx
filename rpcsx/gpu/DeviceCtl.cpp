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

void DeviceCtl::start() { mDevice->start(); }
void DeviceCtl::waitForIdle() { mDevice->waitForIdle(); }
