#include "dmem.hpp"
#include "KernelAllocator.hpp"
#include "KernelObject.hpp"
#include "error.hpp"
#include "kernel/KernelObject.hpp"
#include "pmem.hpp"
#include "rx/AddressRange.hpp"
#include "rx/FileLock.hpp"
#include "rx/Serializer.hpp"
#include "rx/die.hpp"
#include "rx/print.hpp"
#include "utils/Logs.hpp"
#include "vmem.hpp"
#include <array>
#include <rx/format.hpp>
#include <string_view>
#include <utility>

struct DirectMemoryAllocation {
  static constexpr std::uint32_t kAllocatedBit = 1 << 31;
  static constexpr std::uint32_t kPooledBit = 1 << 30;
  std::uint32_t type = 0;

  [[nodiscard]] bool isPooled() const { return type & kPooledBit; }
  void markAsPooled() { type |= kPooledBit | kAllocatedBit; }

  void setMemoryType(orbis::MemoryType memoryType) {
    type = (std::to_underlying(memoryType) & ~(kAllocatedBit | kPooledBit)) |
           (type & kPooledBit) | kAllocatedBit;
  }

  [[nodiscard]] orbis::MemoryType getMemoryType() const {
    return static_cast<orbis::MemoryType>(type & ~(kAllocatedBit | kPooledBit));
  }

  [[nodiscard]] bool isAllocated() const { return (type & kAllocatedBit) != 0; }
  [[nodiscard]] bool isRelated(const DirectMemoryAllocation &other,
                               rx::AddressRange, rx::AddressRange) const {
    return type == other.type;
  }

  [[nodiscard]] DirectMemoryAllocation
  merge(const DirectMemoryAllocation &other, rx::AddressRange,
        rx::AddressRange) const {
    return other;
  }

  bool operator==(const DirectMemoryAllocation &) const = default;
};

struct DirectMemoryResourceState {
  std::uint64_t pmemOffset{};
  std::uint64_t dmemTotalSize{};
  std::uint64_t dmemReservedSize{};
};

struct DirectMemoryResource
    : kernel::AllocableResource<DirectMemoryAllocation, orbis::kallocator>,
      DirectMemoryResourceState {
  using BaseResource =
      kernel::AllocableResource<DirectMemoryAllocation, orbis::kallocator>;

  void create(std::size_t size) {
    auto [pmemRange, errc] = orbis::pmem::allocate(0, size, {}, 64 * 1024);

    rx::dieIf(errc != orbis::ErrorCode{},
              "failed to allocate direct memory: size {:x}, error {}", size,
              static_cast<int>(errc));

    dmemTotalSize = pmemRange.size();
    pmemOffset = pmemRange.beginAddress();
    auto dmemResourceRange =
        rx::AddressRange::fromBeginSize(0, pmemRange.size());

    if (auto errc = BaseResource::create(dmemResourceRange);
        errc != std::errc{}) {
      rx::die("failed to create direct memory resource {:x}, error {}",
              pmemRange.size(), errc);
    }
  }

  orbis::ErrorCode clear() {
    auto result = BaseResource::create(
        rx::AddressRange::fromBeginSize(0, dmemTotalSize + dmemReservedSize));

    if (result != std::errc{}) {
      return orbis::toErrorCode(result);
    }
    dmemReservedSize = 0;
    return {};
  }

  void serialize(rx::Serializer &s) const {
    s.serialize(static_cast<const DirectMemoryResourceState &>(*this));
    BaseResource::serialize(s);
  }

  void deserialize(rx::Deserializer &d) {
    d.deserialize(static_cast<DirectMemoryResourceState &>(*this));
    BaseResource::deserialize(d);
  }
};

static std::array g_dmemPools = {
    orbis::createGlobalObject<
        kernel::LockableKernelObject<DirectMemoryResource>>(),
    orbis::createGlobalObject<
        kernel::LockableKernelObject<DirectMemoryResource>>(),
    orbis::createGlobalObject<
        kernel::LockableKernelObject<DirectMemoryResource>>(),
};

orbis::ErrorCode orbis::dmem::initialize() {
  g_dmemPools[0]->create(0x120000000);
  g_dmemPools[1]->create(0x1000000);
  g_dmemPools[2]->create(0x1000000);

  return {};
}

orbis::ErrorCode orbis::dmem::clear(unsigned dmemIndex) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return ErrorCode::INVAL;
  }

  {
    auto dmem = g_dmemPools[dmemIndex];
    std::lock_guard lock(*dmem);
    auto result = dmem->clear();
    if (result != orbis::ErrorCode{}) {
      return result;
    }
  }

  return {};
}

static void dmemDump(unsigned dmemIndex, std::string_view message = {}) {
  auto dmem = g_dmemPools[dmemIndex];

  rx::ScopedFileLock lock(stderr);

  rx::println(stderr, "dmem0 {}", message);
  for (auto alloc : *dmem) {
    rx::println(stderr, "  {:012x}-{:012x}: {}{} {}", alloc.beginAddress(),
                alloc.endAddress(), "_A"[alloc->isAllocated()],
                "_P"[alloc->isPooled()], alloc->getMemoryType());
  }
}

std::pair<std::uint64_t, orbis::ErrorCode>
orbis::dmem::allocate(unsigned dmemIndex, rx::AddressRange searchRange,
                      std::uint64_t len, MemoryType memoryType,
                      std::uint64_t alignment, bool pooled) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return {{}, ErrorCode::INVAL};
  }

  if (searchRange.endAddress() != 0 &&
      (!searchRange.isValid() || searchRange.size() < len)) {
    ORBIS_LOG_ERROR(__FUNCTION__, "invalid range", searchRange.beginAddress(),
                    searchRange.endAddress(), len);
    return {{}, orbis::ErrorCode::INVAL};
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  alignment = alignment == 0 ? kPageSize : alignment;
  len = rx::alignUp(len, dmem::kPageSize);

  if (searchRange.endAddress() == 0) {
    searchRange = rx::AddressRange::fromBeginEnd(searchRange.beginAddress(),
                                                 dmem->dmemTotalSize -
                                                     dmem->dmemReservedSize);
  }

  if (!searchRange.isValid() || searchRange.size() < len) {
    ORBIS_LOG_ERROR(__FUNCTION__, "invalid range", searchRange.beginAddress(),
                    searchRange.endAddress(), len);
    return {{}, orbis::ErrorCode::INVAL};
  }

  if (len == 0) {
    ORBIS_LOG_ERROR(__FUNCTION__, "len is 0", searchRange.beginAddress(),
                    searchRange.endAddress(), len);

    return {{}, ErrorCode::INVAL};
  }

  if (searchRange.endAddress() > dmem->dmemTotalSize - dmem->dmemReservedSize) {
    ORBIS_LOG_ERROR(__FUNCTION__, "out of direct memory size",
                    searchRange.beginAddress(), searchRange.endAddress(),
                    dmem->dmemTotalSize);
    return {{}, ErrorCode::INVAL};
  }

  DirectMemoryAllocation allocation;
  allocation.setMemoryType(memoryType);
  if (pooled) {
    allocation.markAsPooled();
  }

  auto allocResult = dmem->map(searchRange.beginAddress(), len, allocation,
                               AllocationFlags::Dry, alignment);
  if (allocResult.errc != std::errc{}) {
    return {{}, toErrorCode(allocResult.errc)};
  }

  auto result = allocResult.range.beginAddress();

  auto commitResult =
      dmem->map(result, len, allocation, AllocationFlags::Fixed, alignment);

  rx::dieIf(commitResult.errc != std::errc{},
            "dmem: failed to commit memory, error {}", commitResult.errc);

  dmemDump(dmemIndex,
           rx::format("allocated {:x}-{:x}", allocResult.range.beginAddress(),
                      allocResult.range.endAddress()));

  return {result, {}};
}

orbis::ErrorCode orbis::dmem::release(unsigned dmemIndex,
                                      rx::AddressRange range, bool pooled) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return ErrorCode::INVAL;
  }

  if (range.beginAddress() % kPageSize || range.endAddress() % kPageSize ||
      !range.isValid()) {
    return ErrorCode::INVAL;
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  constexpr auto razorGpuMemory =
      rx::AddressRange::fromBeginSize(0x3000000000, 0x20000000);

  if (dmemIndex == 0 && razorGpuMemory.contains(range.beginAddress())) {
    return ErrorCode::OPNOTSUPP;
  }

  auto it = dmem->query(range.beginAddress());

  if (it == dmem->end() || !it->isAllocated()) {
    return ErrorCode::NOENT;
  }

  if (it->isPooled() && !pooled) {
    return ErrorCode::NOENT;
  }

  DirectMemoryAllocation allocation{};
  auto result = dmem->map(range.beginAddress(), range.size(), allocation,
                          AllocationFlags::Fixed, vmem::kPageSize);

  if (result.errc != std::errc{}) {
    return toErrorCode(result.errc);
  }

  return {};
}

std::pair<std::uint64_t, orbis::ErrorCode>
orbis::dmem::reserveSystem(unsigned dmemIndex, std::uint64_t size) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return {{}, ErrorCode::INVAL};
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  if (size == 0 || size % vmem::kPageSize) {
    return {{}, ErrorCode::INVAL};
  }

  if (dmem->dmemReservedSize + size > dmem->dmemTotalSize) {
    return {{}, ErrorCode::NOMEM};
  }

  DirectMemoryAllocation alloc;
  alloc.setMemoryType(MemoryType::WbOnion);
  auto result = dmem->map(0, size, alloc, AllocationFlags::Stack, kPageSize);

  if (result.errc != std::errc{}) {
    return {{}, toErrorCode(result.errc)};
  }

  dmem->dmemReservedSize += size;
  return {result.range.beginAddress() | 0x4000000000, {}};
}

std::pair<std::uint64_t, orbis::ErrorCode>
orbis::dmem::reservePooled(unsigned dmemIndex, rx::AddressRange searchRange,
                           std::uint64_t size, std::uint64_t alignment) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return {{}, ErrorCode::INVAL};
  }

  alignment = alignment == 0 ? kPageSize : alignment;

  if (alignment % kPageSize) {
    return {{}, ErrorCode::INVAL};
  }

  auto dmem = g_dmemPools[dmemIndex];

  if (searchRange.endAddress() > dmem->dmemTotalSize) {
    ORBIS_LOG_ERROR(__FUNCTION__, "out of direct memory size",
                    searchRange.endAddress(), dmem->dmemTotalSize);
    return {{}, ErrorCode::INVAL};
  }

  if (!searchRange.isValid() &&
      searchRange.beginAddress() != searchRange.endAddress()) {
    ORBIS_LOG_ERROR(__FUNCTION__, "invalid range", searchRange.beginAddress(),
                    searchRange.endAddress());
    return {{}, ErrorCode::INVAL};
  }

  std::lock_guard lock(*dmem);

  if (searchRange.beginAddress() == searchRange.endAddress()) {
    searchRange = rx::AddressRange::fromBeginEnd(searchRange.beginAddress(),
                                                 dmem->dmemTotalSize -
                                                     dmem->dmemReservedSize);

    if (!searchRange.isValid()) {
      ORBIS_LOG_ERROR(__FUNCTION__, "invalid range", searchRange.beginAddress(),
                      searchRange.endAddress());
      return {{}, ErrorCode::INVAL};
    }
  }

  if (searchRange.endAddress() > dmem->dmemTotalSize - dmem->dmemReservedSize) {
    ORBIS_LOG_ERROR(__FUNCTION__, "out of direct memory size",
                    searchRange.endAddress(), dmem->dmemTotalSize,
                    dmem->dmemReservedSize);
    return {{}, ErrorCode::INVAL};
  }

  auto it = dmem->lowerBound(searchRange.beginAddress());
  rx::AddressRange result;

  while (it != dmem->end() && it.beginAddress() < searchRange.endAddress()) {
    if (!it->isAllocated()) {
      auto viewRange = searchRange.intersection(it.range());

      viewRange = rx::AddressRange::fromBeginEnd(
          rx::alignUp(viewRange.beginAddress(), alignment),
          viewRange.endAddress());

      if (viewRange.isValid() && viewRange.size() >= size) {
        result =
            rx::AddressRange::fromBeginSize(viewRange.beginAddress(), size);
        break;
      }
    }

    ++it;
  }

  if (!result.isValid()) {
    return {{}, ErrorCode::NOMEM};
  }

  DirectMemoryAllocation allocation;
  allocation.markAsPooled();

  auto commitResult = dmem->map(result.beginAddress(), result.size(),
                                allocation, AllocationFlags::Fixed, alignment);

  if (commitResult.errc != std::errc{}) {
    return {{}, toErrorCode(commitResult.errc)};
  }

  return {result.beginAddress(), {}};
}

orbis::ErrorCode orbis::dmem::setType(unsigned dmemIndex,
                                      rx::AddressRange range, MemoryType type,
                                      rx::EnumBitSet<QueryFlags> flags) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return ErrorCode::INVAL;
  }

  bool expectedPooled = (flags & QueryFlags::Pooled) == QueryFlags::Pooled;

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  auto it = dmem->query(range.beginAddress());

  if (it == dmem->end() || !it->isAllocated() ||
      it->isPooled() != expectedPooled) {
    return ErrorCode::ACCES;
  }

  if (it->getMemoryType() == type && it.range().contains(range)) {
    return {};
  }

  DirectMemoryAllocation allocation;
  allocation.setMemoryType(type);
  auto [_it, errc, _range] =
      dmem->map(range.beginAddress(), range.size(), allocation,
                AllocationFlags::Fixed, vmem::kPageSize);

  return toErrorCode(errc);
}

std::optional<orbis::dmem::QueryResult>
orbis::dmem::query(unsigned dmemIndex, std::uint64_t dmemOffset,
                   rx::EnumBitSet<QueryFlags> flags) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return {};
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  auto it = dmem->lowerBound(dmemOffset);

  if (flags & QueryFlags::LowerBound) {
    while (it != dmem->end() && !it->isAllocated()) {
      ++it;
    }

    if (it == dmem->end()) {
      return {};
    }
  } else if (it == dmem->end() || !it->isAllocated()) {
    return {};
  }

  if (!(flags & QueryFlags::Pooled)) {
    if (it->isPooled()) {
      return {};
    }
  }

  return QueryResult{.range = it.range(), .memoryType = it->getMemoryType()};
}

std::uint64_t orbis::dmem::getSize(unsigned dmemIndex) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return 0;
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  return dmem->dmemTotalSize - dmem->dmemReservedSize;
}

std::pair<rx::AddressRange, orbis::ErrorCode>
orbis::dmem::getAvailSize(unsigned dmemIndex, rx::AddressRange searchRange,
                          std::uint64_t alignment) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return {{}, ErrorCode::INVAL};
  }

  alignment = alignment == 0 ? kPageSize : alignment;

  if (alignment % kPageSize) {
    return {{}, ErrorCode::INVAL};
  }

  auto dmem = g_dmemPools[dmemIndex];

  if (searchRange.endAddress() > dmem->dmemTotalSize) {
    ORBIS_LOG_ERROR(__FUNCTION__, "out of direct memory size",
                    searchRange.endAddress(), dmem->dmemTotalSize);
    return {{}, orbis::ErrorCode::INVAL};
  }

  if (!searchRange.isValid() &&
      searchRange.beginAddress() != searchRange.endAddress()) {
    ORBIS_LOG_ERROR(__FUNCTION__, "invalid range", searchRange.beginAddress(),
                    searchRange.endAddress());
    return {{}, orbis::ErrorCode::INVAL};
  }

  std::lock_guard lock(*dmem);

  if (searchRange.beginAddress() == searchRange.endAddress()) {
    searchRange = rx::AddressRange::fromBeginEnd(searchRange.beginAddress(),
                                                 dmem->dmemTotalSize -
                                                     dmem->dmemReservedSize);

    if (!searchRange.isValid()) {
      ORBIS_LOG_ERROR(__FUNCTION__, "invalid range", searchRange.beginAddress(),
                      searchRange.endAddress());
      return {{}, orbis::ErrorCode::INVAL};
    }
  }

  if (searchRange.endAddress() > dmem->dmemTotalSize - dmem->dmemReservedSize) {
    ORBIS_LOG_ERROR(__FUNCTION__, "out of direct memory size",
                    searchRange.endAddress(), dmem->dmemTotalSize,
                    dmem->dmemReservedSize);
    return {{}, orbis::ErrorCode::INVAL};
  }

  auto it = dmem->lowerBound(searchRange.beginAddress());
  rx::AddressRange result = rx::AddressRange::fromBeginEnd(
      searchRange.beginAddress(), searchRange.beginAddress());

  while (it != dmem->end() && it.beginAddress() < searchRange.endAddress()) {
    if (!it->isAllocated()) {
      auto viewRange = searchRange.intersection(it.range());

      viewRange = rx::AddressRange::fromBeginEnd(
          rx::alignUp(viewRange.beginAddress(), alignment),
          viewRange.endAddress());

      if (viewRange.isValid() &&
          (!result.isValid() || viewRange.size() > result.size())) {
        result = viewRange;
      }
    }

    ++it;
  }

  return {result, {}};
}

orbis::ErrorCode orbis::dmem::map(unsigned dmemIndex, rx::AddressRange range,
                                  std::uint64_t offset,
                                  rx::EnumBitSet<vmem::Protection> protection) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return ErrorCode::INVAL;
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  if (offset < 0 || offset + range.size() < offset ||
      offset + range.size() > dmem->dmemTotalSize) {
    return orbis::ErrorCode::INVAL;
  }

  if (offset + range.size() > dmem->dmemTotalSize - dmem->dmemReservedSize) {
    return orbis::ErrorCode::ACCES;
  }

  auto allocationInfoIt = dmem->query(offset);

  if (allocationInfoIt == dmem->end() || !allocationInfoIt->isAllocated()) {
    if (allocationInfoIt != dmem->end()) {
      dmemDump(
          dmemIndex,
          rx::format("map unallocated {:x}-{:x}, requested range {:x}-{:x}",
                     allocationInfoIt.beginAddress(),
                     allocationInfoIt.endAddress(), range.beginAddress(),
                     range.endAddress()));
    } else {
      dmemDump(dmemIndex, rx::format("map out of memory {:x}-{:x}",
                                     range.beginAddress(), range.endAddress()));
    }
    return orbis::ErrorCode::ACCES;
  }

  if (allocationInfoIt->isPooled()) {
    return orbis::ErrorCode::ACCES;
  }

  auto directRange = rx::AddressRange::fromBeginSize(offset, range.size())
                         .intersection(allocationInfoIt.range());

  if (range.size() > directRange.size()) {
    return orbis::ErrorCode::INVAL;
  }

  auto physicalRange =
      rx::AddressRange::fromBeginSize(dmem->pmemOffset + offset, range.size());

  return orbis::pmem::map(range.beginAddress(), physicalRange,
                          vmem::toCpuProtection(protection));
}

std::pair<std::uint64_t, orbis::ErrorCode>
orbis::dmem::getPmemOffset(unsigned dmemIndex, std::uint64_t dmemOffset) {
  if (dmemIndex > std::size(g_dmemPools)) {
    return {{}, ErrorCode::INVAL};
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  if (dmemOffset >= dmem->dmemTotalSize) {
    return {{}, orbis::ErrorCode::INVAL};
  }

  if (dmemOffset >= dmem->dmemTotalSize - dmem->dmemReservedSize) {
    return {{}, orbis::ErrorCode::ACCES};
  }

  auto allocationInfoIt = dmem->query(dmemOffset);

  if (allocationInfoIt == dmem->end() || !allocationInfoIt->isAllocated()) {
    return {{}, orbis::ErrorCode::ACCES};
  }

  return {dmem->pmemOffset + dmemOffset, {}};
}
