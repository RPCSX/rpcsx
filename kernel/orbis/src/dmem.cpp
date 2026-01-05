#include "dmem.hpp"
#include "KernelAllocator.hpp"
#include "KernelContext.hpp"
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
#include <thread/Process.hpp>
#include <utility>

struct DirectMemoryAllocation {
  static constexpr std::uint32_t kAllocatedBit = 1 << 31;
  static constexpr std::uint32_t kPooledBit = 1 << 30;
  std::uint32_t type = 0;

  struct Mapping {
    orbis::Process *process;
    rx::AddressRange vmRange;
    std::uint64_t dmemOffset;

    void serialize(rx::Serializer &s) const {
      s.serialize(process->pid);
      s.serialize(vmRange);
    }

    void deserialize(rx::Deserializer &d) {
      auto pid = d.deserialize<orbis::pid_t>();
      if (d.failure()) {
        return;
      }
      auto foundProcess = orbis::findProcessById(pid);
      if (foundProcess == nullptr) {
        d.setFailure();
        return;
      }

      process = foundProcess;
      d.deserialize(vmRange);
    }

    bool operator==(const Mapping &) const = default;
  };

  orbis::kvector<Mapping> mappings;

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
    return false;
  }

  [[nodiscard]] DirectMemoryAllocation
  merge(const DirectMemoryAllocation &other, rx::AddressRange,
        rx::AddressRange) const {
    auto result = *this;
    result.mappings.insert(result.mappings.end(), other.mappings.begin(),
                           other.mappings.end());
    return result;
  }

  void unmap(orbis::Process *process, rx::AddressRange range) {
    for (std::size_t i = 0; i < mappings.size(); ++i) {
      auto &map = mappings[i];

      if (process != nullptr && map.process != process) {
        continue;
      }

      auto blockRange = range.intersection(map.vmRange);

      if (!blockRange.isValid()) {
        continue;
      }

      if (map.vmRange == blockRange) {
        if (i != mappings.size() - 1) {
          std::swap(mappings[i], mappings.back());
        }
        mappings.pop_back();
        --i;
        continue;
      }

      if (map.vmRange.beginAddress() == blockRange.beginAddress()) {
        map.vmRange = rx::AddressRange::fromBeginEnd(blockRange.endAddress(),
                                                     map.vmRange.endAddress());
        map.dmemOffset += blockRange.size();
        continue;
      }

      if (map.vmRange.endAddress() == blockRange.endAddress()) {
        map.vmRange = rx::AddressRange::fromBeginEnd(map.vmRange.beginAddress(),
                                                     blockRange.beginAddress());
        continue;
      }

      auto leftAllocation = rx::AddressRange::fromBeginEnd(
          map.vmRange.beginAddress(), blockRange.beginAddress());

      auto rightAllocation = rx::AddressRange::fromBeginEnd(
          blockRange.endAddress(), map.vmRange.endAddress());

      map.vmRange = leftAllocation;
      mappings.push_back({
          .process = process,
          .vmRange = rightAllocation,
          .dmemOffset = map.dmemOffset + (rightAllocation.beginAddress() -
                                          leftAllocation.beginAddress()),
      });
    }
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

  BaseResource systemResource;

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

    systemResource.destroy();
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

  std::pair<std::uint64_t, orbis::ErrorCode> reserveSystem(std::uint64_t size) {
    DirectMemoryAllocation alloc;
    alloc.setMemoryType(orbis::MemoryType::WbOnion);
    auto result = map(0, size, alloc, orbis::AllocationFlags::Stack,
                      orbis::dmem::kPageSize);

    if (result.errc != std::errc{}) {
      return {{}, orbis::toErrorCode(result.errc)};
    }

    allocations.unmap(result.range);
    dmemReservedSize += size;
    systemResource.allocations.map(result.range, {});
    return {result.range.beginAddress(), {}};
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
  g_dmemPools[0]->create(0x180000000);
  g_dmemPools[1]->create(0x1000000);
  g_dmemPools[2]->create(0x1000000);

  g_dmemPools[0]->reserveSystem(0x60000000);

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

  rx::println(stderr, "dmem{} {}", dmemIndex, message);
  for (auto alloc : *dmem) {
    rx::println(stderr, "  {:012x}-{:012x}: {}{} {}", alloc.beginAddress(),
                alloc.endAddress(), "_A"[alloc->isAllocated()],
                "_P"[alloc->isPooled()], alloc->getMemoryType());
    for (auto &map : alloc->mappings) {
      auto dmemRange =
          rx::AddressRange::fromBeginSize(map.dmemOffset, map.vmRange.size());

      rx::println(stderr, "    {:#x}-{:#x} -> {:#x}-{:#x} pid {}",
                  dmemRange.beginAddress(), dmemRange.endAddress(),
                  map.vmRange.beginAddress(), map.vmRange.endAddress(),
                  map.process->pid);
    }
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

  if (searchRange.endAddress() == 0 ||
      searchRange.endAddress() > dmem->dmemTotalSize - dmem->dmemReservedSize) {
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
    return {{}, ErrorCode::AGAIN};
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

std::pair<std::uint64_t, orbis::ErrorCode>
orbis::dmem::allocateSystem(unsigned dmemIndex, std::uint64_t len,
                            MemoryType memoryType, std::uint64_t alignment) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return {{}, ErrorCode::INVAL};
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  alignment = alignment == 0 ? kPageSize : alignment;
  len = rx::alignUp(len, dmem::kPageSize);

  DirectMemoryAllocation allocation;
  allocation.setMemoryType(memoryType);

  auto allocResult = dmem->systemResource.map(0, len, allocation,
                                              AllocationFlags::Dry, alignment);
  if (allocResult.errc != std::errc{}) {
    return {{}, ErrorCode::AGAIN};
  }

  auto result = allocResult.range.beginAddress();

  auto commitResult =
      dmem->map(result, len, allocation, AllocationFlags::Fixed, alignment);

  rx::dieIf(commitResult.errc != std::errc{},
            "dmem: failed to commit main memory, error {}", commitResult.errc);

  dmemDump(dmemIndex, rx::format("allocated main {:x}-{:x}",
                                 allocResult.range.beginAddress(),
                                 allocResult.range.endAddress()));

  return {allocResult.range.beginAddress() | 0x4000000000, {}};
}

orbis::ErrorCode orbis::dmem::release(unsigned dmemIndex,
                                      rx::AddressRange range, bool pooled) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return ErrorCode::INVAL;
  }

  if (range.beginAddress() % kPageSize || range.size() % vmem::kPageSize ||
      !range.isValid()) {
    return ErrorCode::INVAL;
  }

  orbis::kvector<DirectMemoryAllocation::Mapping> clearMappings;
  {
    auto dmem = g_dmemPools[dmemIndex];
    std::lock_guard lock(*dmem);

    constexpr auto razorGpuMemory =
        rx::AddressRange::fromBeginSize(0x3000000000, 0x20000000);

    if (dmemIndex == 0 && razorGpuMemory.contains(range.beginAddress())) {
      return ErrorCode::OPNOTSUPP;
    }

    auto beginIt = dmem->query(range.beginAddress());

    if (beginIt == dmem->end()) {
      return ErrorCode::NOENT;
    }

    auto endIt = beginIt;
    while (endIt != dmem->end() && endIt.beginAddress() < range.endAddress()) {
      if (!beginIt->isAllocated()) {
        return ErrorCode::NOENT;
      }

      if (endIt->isPooled() && !pooled) {
        return ErrorCode::NOENT;
      }

      ++endIt;
    }

    for (auto it = beginIt; it != endIt; ++it) {
      for (auto &mapping : it->mappings) {
        auto mapRange = rx::AddressRange::fromBeginSize(mapping.dmemOffset,
                                                        mapping.vmRange.size());
        auto releaseRange = mapRange.intersection(range);

        if (!releaseRange.isValid()) {
          continue;
        }

        auto releaseVirtualRange = rx::AddressRange::fromBeginSize(
            mapping.vmRange.beginAddress() +
                (releaseRange.beginAddress() - mapRange.beginAddress()),
            releaseRange.size());

        clearMappings.push_back({
            .process = mapping.process,
            .vmRange = releaseVirtualRange,
        });
      }

      it->unmap(nullptr, range);
    }

    DirectMemoryAllocation allocation{};
    auto result = dmem->map(range.beginAddress(), range.size(), allocation,
                            AllocationFlags::Fixed, vmem::kPageSize);

    if (result.errc != std::errc{}) {
      return toErrorCode(result.errc);
    }

    dmemDump(dmemIndex, rx::format("released {:x}-{:x}", range.beginAddress(),
                                   range.endAddress()));
  }

  for (auto mapping : clearMappings) {
    mapping.process->invoke(
        [=] { vmem::unmap(mapping.process, mapping.vmRange); });
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

  return dmem->reserveSystem(size);
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

  if (alignment % vmem::kPageSize) {
    return {{}, ErrorCode::INVAL};
  }

  auto dmem = g_dmemPools[dmemIndex];

  if (searchRange.endAddress() > dmem->dmemTotalSize) {
    ORBIS_LOG_ERROR(__FUNCTION__, "out of direct memory size",
                    searchRange.endAddress(), dmem->dmemTotalSize);
    // return {{}, orbis::ErrorCode::INVAL};
    searchRange = rx::AddressRange::fromBeginEnd(searchRange.beginAddress(),
                                                 dmem->dmemTotalSize);
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
    searchRange = rx::AddressRange::fromBeginEnd(searchRange.beginAddress(),
                                                 dmem->dmemTotalSize -
                                                     dmem->dmemReservedSize);

    // return {{}, orbis::ErrorCode::INVAL};
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

orbis::ErrorCode orbis::dmem::map(orbis::Process *process, unsigned dmemIndex,
                                  rx::AddressRange range, std::uint64_t offset,
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
    dmemDump(dmemIndex,
             rx::format("map out of memory {:x}-{:x}, dmem size {:x}",
                        range.beginAddress(), range.endAddress(),
                        dmem->dmemTotalSize - dmem->dmemReservedSize));
    return orbis::ErrorCode::ACCES;
  }

  auto beginIt = dmem->query(offset);

  if (beginIt == dmem->end()) {
    dmemDump(dmemIndex, rx::format("map out of memory {:x}-{:x}",
                                   range.beginAddress(), range.endAddress()));
    return orbis::ErrorCode::ACCES;
  }

  auto endIt = beginIt;
  while (endIt != dmem->end() && endIt.beginAddress() < offset + range.size()) {
    if (!endIt->isAllocated() || endIt->isPooled()) {
      dmemDump(dmemIndex,
               rx::format("attempt to map unallocated or pulled memory "
                          "{:x}-{:x}, dmem size {:x}",
                          range.beginAddress(), range.endAddress(),
                          dmem->dmemTotalSize - dmem->dmemReservedSize));
      return orbis::ErrorCode::ACCES;
    }

    if (!vmem::validateMemoryType(endIt->getMemoryType(), protection)) {
      dmemDump(dmemIndex, rx::format("invalid protection {} for memory type {}",
                                     protection, endIt->getMemoryType()));
      return ErrorCode::ACCES;
    }

    // if (!endIt->mappings.empty() && !process->allowDmemAliasing) {
    //   return ErrorCode::INVAL;
    // }

    ++endIt;
  }

  if (auto last = endIt; (--last).endAddress() < offset + range.size()) {
    dmemDump(dmemIndex,
             rx::format("map out of memory {:x}-{:x}, dmem size {:x}",
                        range.beginAddress(), range.endAddress(),
                        dmem->dmemTotalSize - dmem->dmemReservedSize));
    return orbis::ErrorCode::ACCES;
  }

  auto dmemRange = rx::AddressRange::fromBeginSize(offset, range.size());

  for (auto it = beginIt; it != endIt; ++it) {
    auto itRange = it.range();
    auto mappingDmemRange = dmemRange.intersection(itRange);
    auto mappingVmemRange = rx::AddressRange::fromBeginSize(
        range.beginAddress() + (mappingDmemRange.beginAddress() - offset),
        mappingDmemRange.size());

    it->mappings.push_back({
        .process = process,
        .vmRange = mappingVmemRange,
        .dmemOffset = mappingDmemRange.beginAddress(),
    });
  }

  dmemDump(dmemIndex,
           rx::format("map {:#x}-{:#x} -> {:#x}-{:#x} pid {}",
                      dmemRange.beginAddress(), dmemRange.endAddress(),
                      range.beginAddress(), range.endAddress(), process->pid));
  auto physicalRange =
      rx::AddressRange::fromBeginSize(dmem->pmemOffset + offset, range.size());

  auto result = process->invoke([=] {
    return orbis::pmem::map(range.beginAddress(), physicalRange,
                            vmem::toCpuProtection(protection));
  });

  rx::dieIf(result != ErrorCode{}, "failed to map physical memory");
  return {};
}

orbis::ErrorCode orbis::dmem::notifyUnmap(orbis::Process *process,
                                          unsigned dmemIndex,
                                          std::uint64_t offset,
                                          rx::AddressRange range) {
  if (dmemIndex >= std::size(g_dmemPools)) {
    return ErrorCode::INVAL;
  }

  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  auto it = dmem->lowerBound(offset);

  while (it != dmem->end() && it.beginAddress() < offset + range.size()) {
    it->unmap(process, range);
    ++it;
  }

  dmemDump(dmemIndex, rx::format("unmap {:#x}-{:#x}", range.beginAddress(),
                                 range.endAddress()));

  return {};
}

orbis::ErrorCode orbis::dmem::protect(orbis::Process *process,
                                      unsigned dmemIndex,
                                      rx::AddressRange range,
                                      rx::EnumBitSet<vmem::Protection> prot) {
  auto dmem = g_dmemPools[dmemIndex];
  std::lock_guard lock(*dmem);

  auto it = dmem->query(range.beginAddress());
  if (it == dmem->end()) {
    return ErrorCode::INVAL;
  }

  if (!it.range().contains(range)) {
    return ErrorCode::INVAL;
  }

  for (auto mapping : it->mappings) {
    if (process == nullptr || process == mapping.process) {
      vmem::protect(mapping.process, mapping.vmRange, prot);
    }
  }

  return {};
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
