#include "vmem.hpp"
#include "IoDevice.hpp"
#include "KernelObject.hpp"
#include "error.hpp"
#include "pmem.hpp"
#include "rx/Mappable.hpp"
#include "rx/Rc.hpp"
#include "rx/StaticString.hpp"
#include "rx/mem.hpp"
#include "rx/print.hpp"
#include "thread/Process.hpp"
#include <algorithm>
#include <cstdio>
#include <mutex>

struct VirtualMemoryAllocation {
  rx::EnumBitSet<orbis::vmem::BlockFlags> flags{};
  rx::EnumBitSet<orbis::vmem::Protection> prot{};
  rx::Ref<orbis::IoDevice> device;
  std::uint64_t deviceOffset = 0;
  rx::StaticString<31> name;

  [[nodiscard]] bool isAllocated() const {
    return (flags & orbis::vmem::BlockFlags::Allocated) ==
           orbis::vmem::BlockFlags::Allocated;
  }

  [[nodiscard]] bool
  isRelated(const VirtualMemoryAllocation &other, rx::AddressRange selfRange,
            [[maybe_unused]] rx::AddressRange rightRange) const {
    if (flags != other.flags || prot != other.prot || device != other.device ||
        name != other.name) {
      return false;
    }

    return !isAllocated() ||
           deviceOffset + selfRange.size() == other.deviceOffset;
  }

  [[nodiscard]] VirtualMemoryAllocation merge(const VirtualMemoryAllocation &,
                                              rx::AddressRange,
                                              rx::AddressRange) const {
    return *this;
  }

  std::pair<rx::AddressRange, VirtualMemoryAllocation>
  truncate(rx::AddressRange selfRange, rx::AddressRange,
           rx::AddressRange rightRange) {
    if (!isAllocated() || !rightRange.isValid() || device == nullptr) {
      return {};
    }

    // adjust deviceOffset for new right node
    auto result = *this;

    result.deviceOffset =
        rightRange.beginAddress() - selfRange.beginAddress() + deviceOffset;
    return {rightRange, std::move(result)};
  }

  bool operator==(const VirtualMemoryAllocation &) const = default;
};

using MappableMemoryResource =
    kernel::MappableResource<decltype([](std::size_t size) {
      return rx::Mappable::CreateMemory(size);
    })>;

struct VirtualMemoryResource
    : kernel::AllocableResource<VirtualMemoryAllocation> {};

static auto g_vmInstance = orbis::createProcessLocalObject<
    kernel::LockableKernelObject<VirtualMemoryResource>>();

void orbis::vmem::initialize(Process *process, bool force) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  // FIXME: for PS5 should be extended range
  auto range = rx::AddressRange::fromBeginEnd(0x400000, 0x10000000000);
  vmem->create(range);

  std::size_t address = range.beginAddress();
  auto alignment = std::max<std::size_t>(rx::mem::pageSize, kPageSize);

  auto reserveRangeImpl = [&](rx::AddressRange reserveRange) {
    {
      auto virtualReserveRange = rx::AddressRange::fromBeginEnd(
          rx::alignDown(reserveRange.beginAddress(), kPageSize),
          rx::alignUp(reserveRange.endAddress(), kPageSize));
      vmem->allocations.map(virtualReserveRange, {});
    }

    reserveRange = rx::AddressRange::fromBeginEnd(
        rx::alignUp(reserveRange.beginAddress(), alignment),
        rx::alignDown(reserveRange.endAddress(), alignment));

    if (!reserveRange.isValid() || reserveRange.size() < alignment) {
      return;
    }

    if (force) {
      rx::mem::release(reserveRange, kPageSize);
    }

    if (auto reserveResult = rx::mem::reserve(reserveRange);
        reserveResult != std::errc{} && !force) {
      rx::die("failed to reserve memory {:x}-{:x}", reserveRange.beginAddress(),
              reserveRange.endAddress());
    }
  };

  for (auto usedRange : rx::mem::query(range)) {
    reserveRangeImpl(
        rx::AddressRange::fromBeginEnd(address, usedRange.beginAddress()));

    address = usedRange.endAddress();
  }

  reserveRangeImpl(rx::AddressRange::fromBeginEnd(address, range.endAddress()));
}

void orbis::vmem::fork(Process *process, Process *parentThread) {
  // FIXME: implement
}

std::pair<rx::AddressRange, orbis::ErrorCode> orbis::vmem::map(
    Process *process, std::uint64_t addressHint, std::uint64_t size,
    rx::EnumBitSet<AllocationFlags> allocFlags, rx::EnumBitSet<Protection> prot,
    rx::EnumBitSet<BlockFlags> blockFlags, std::uint64_t alignment,
    std::string_view name, IoDevice *device, std::int64_t deviceOffset) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  VirtualMemoryAllocation allocationInfo;
  allocationInfo.flags = blockFlags | orbis::vmem::BlockFlags::Allocated;
  allocationInfo.device = device;
  allocationInfo.prot = prot;
  allocationInfo.deviceOffset = deviceOffset;
  allocationInfo.name = name;

  if (device == nullptr) {
    if (deviceOffset != 0 || prot) {
      return {{}, ErrorCode::INVAL};
    }

    auto [_, errc, range] =
        vmem->map(addressHint, size, allocationInfo, allocFlags, alignment);

    if (errc != std::errc{}) {
      return {{}, toErrorCode(errc)};
    }

    return {range, {}};
  }

  auto [_, errc, range] =
      vmem->map(addressHint, size, allocationInfo,
                allocFlags | AllocationFlags::Dry, alignment);

  if (errc != std::errc{}) {
    if (errc == std::errc::not_enough_memory) {
      // virtual memory shouldn't care about physical memory
      return {{}, orbis::ErrorCode::INVAL};
    }

    return {{}, toErrorCode(errc)};
  }

  rx::EnumBitSet<rx::mem::Protection> deviceProtection = {};

  if (prot & Protection::CpuRead) {
    deviceProtection |= rx::mem::Protection::R;
  }

  if (prot & Protection::CpuWrite) {
    deviceProtection |= rx::mem::Protection::W;
  }

  if (prot & Protection::CpuExec) {
    deviceProtection |= rx::mem::Protection::X;
  }

  if (auto error = device->map(range, deviceOffset, deviceProtection, process);
      error != ErrorCode{}) {
    return {{}, error};
  }

  auto [it, _errc, _range] =
      vmem->map(range.beginAddress(), range.size(), allocationInfo,
                AllocationFlags::Fixed | AllocationFlags::NoMerge, alignment);

  if (name.empty()) {
    it->name = rx::format("anon:{:012x}", it.beginAddress());
  }

  return {range, {}};
}

orbis::ErrorCode orbis::vmem::setName(Process *process, rx::AddressRange range,
                                      std::string_view name) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  auto it = vmem->query(range.beginAddress());

  if (it == vmem->end()) {
    rx::println(stderr,
                "vmem: attempt to set name of invalid address range: "
                "{:x}-{:x}, name: {}",
                range.beginAddress(), range.endAddress(), name);
    return orbis::ErrorCode::INVAL;
  }

  if (!it->isAllocated()) {
    rx::println(stderr,
                "vmem: attempt to set name of unallocated range: request "
                "{:x}-{:x}, node: {:x}-{:x}, name {}",
                range.beginAddress(), range.endAddress(), it.beginAddress(),
                it.endAddress(), name);
    return orbis::ErrorCode::INVAL;
  }

  if (it.range() != range) {
    rx::println(stderr,
                "vmem: set name range mismatch "
                "{:x}-{:x}, node: {:x}-{:x}, name {}",
                range.beginAddress(), range.endAddress(), it.beginAddress(),
                it.endAddress(), name);
  }

  it->name = name;
  return {};
}

orbis::ErrorCode orbis::vmem::unmap(Process *process, rx::AddressRange range) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);
  VirtualMemoryAllocation allocationInfo{};
  auto [it, errc, _] =
      vmem->map(range.beginAddress(), range.endAddress(), allocationInfo,
                AllocationFlags::Fixed, kPageSize);

  rx::mem::release(range, kPageSize);
  return toErrorCode(errc);
}

std::optional<orbis::vmem::QueryResult>
orbis::vmem::query(Process *process, std::uint64_t address) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  auto it = vmem->query(address);
  if (it == vmem->end()) {
    return {};
  }

  orbis::vmem::QueryResult result{};
  result.start = it.beginAddress();
  result.end = it.endAddress();

  if (it->isAllocated()) {
    if (it->flags == BlockFlags::DirectMemory) {
      result.offset = it->deviceOffset;
    }

    result.protection = it->prot.toUnderlying();
    result.flags = it->flags.toUnderlying();
    result.name = it->name;
  }

  return result;
}

std::optional<orbis::vmem::MemoryProtection>
orbis::vmem::queryProtection(Process *process, std::uint64_t address) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  auto it = vmem->query(address);
  if (it == vmem->end()) {
    return {};
  }

  orbis::vmem::MemoryProtection result{};
  result.startAddress = it.beginAddress();
  result.endAddress = it.endAddress();
  result.prot = it->prot.toUnderlying();
  return result;
}
