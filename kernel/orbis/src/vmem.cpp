#include "vmem.hpp"
#include "Budget.hpp"
#include "IoDevice.hpp"
#include "KernelAllocator.hpp"
#include "KernelContext.hpp"
#include "KernelObject.hpp"
#include "MemoryType.hpp"
#include "blockpool.hpp"
#include "dmem.hpp"
#include "error.hpp"
#include "fmem.hpp"
#include "pmem.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include "rx/FileLock.hpp"
#include "rx/FunctionRef.hpp"
#include "rx/Rc.hpp"
#include "rx/Serializer.hpp"
#include "rx/StaticString.hpp"
#include "rx/align.hpp"
#include "rx/die.hpp"
#include "rx/format.hpp"
#include "rx/mem.hpp"
#include "rx/print.hpp"
#include "thread/Process.hpp"
#include <algorithm>
#include <cstdio>
#include <mutex>

// FIXME: remove
namespace amdgpu {
void mapMemory(std::uint32_t pid, rx::AddressRange virtualRange,
               orbis::MemoryType memoryType,
               rx::EnumBitSet<orbis::vmem::Protection> prot,
               std::uint64_t offset);
void unmapMemory(std::uint32_t pid, rx::AddressRange virtualRange);
void protectMemory(std::uint32_t pid, rx::AddressRange virtualRange,
                   rx::EnumBitSet<orbis::vmem::Protection> prot);
} // namespace amdgpu

struct VirtualMemoryAllocation {
  rx::EnumBitSet<orbis::vmem::BlockFlags> flags{};
  rx::EnumBitSet<orbis::vmem::BlockFlagsEx> flagsEx{};
  rx::EnumBitSet<orbis::vmem::Protection> prot{};
  orbis::MemoryType type = orbis::MemoryType::Invalid;
  rx::Ref<orbis::IoDevice> device;
  std::uint64_t deviceOffset = 0;
  rx::StaticString<31> name;

  [[nodiscard]] bool isAllocated() const {
    return (flagsEx & orbis::vmem::BlockFlagsEx::Allocated) ==
           orbis::vmem::BlockFlagsEx::Allocated;
  }

  [[nodiscard]] bool
  isRelated(const VirtualMemoryAllocation &other, rx::AddressRange selfRange,
            [[maybe_unused]] rx::AddressRange rightRange) const {
    if (flags != other.flags || flagsEx != other.flagsEx ||
        prot != other.prot || type != other.type || device != other.device ||
        name != other.name) {
      return false;
    }
    if (!isAllocated()) {
      return true;
    }

    if (device == nullptr || flags == orbis::vmem::BlockFlags::PooledMemory) {
      return true;
    }

    return deviceOffset + selfRange.size() == other.deviceOffset;
  }

  [[nodiscard]] VirtualMemoryAllocation merge(const VirtualMemoryAllocation &,
                                              rx::AddressRange,
                                              rx::AddressRange) const {
    return *this;
  }

  [[nodiscard]] std::pair<rx::AddressRange, VirtualMemoryAllocation>
  truncate(rx::AddressRange selfRange, rx::AddressRange leftRange,
           rx::AddressRange rightRange) const {
    if (!rightRange.isValid() || device == nullptr) {
      return {};
    }

    // adjust deviceOffset for new right node
    auto result = *this;

    result.deviceOffset =
        rightRange.beginAddress() - selfRange.beginAddress() + deviceOffset;
    return {rightRange, std::move(result)};
  }

  bool operator==(const VirtualMemoryAllocation &) const = default;

  void setName(orbis::Process *process, std::string_view newName) {
    if (!newName.empty()) {
      name = newName;
      return;
    }

    name = "(NoName)";

    if (auto executable = process->modulesMap.get(orbis::ModuleHandle{})) {
      name += executable->soName;
    }
  }

  void serialize(rx::Serializer &s) const {}
  void deserialize(rx::Deserializer &d) {}
};

struct VirtualMemoryResource
    : kernel::AllocableResource<VirtualMemoryAllocation, orbis::kallocator> {};

static auto g_vmInstance = orbis::createProcessLocalObject<
    kernel::LockableKernelObject<VirtualMemoryResource>>();

static void vmemDump(orbis::Process *process, std::string_view message = {}) {
  rx::ScopedFileLock lock(stderr);
  auto vmem = process->get(g_vmInstance);

  rx::println(stderr, "vmem {} {}", process->pid, message);
  for (auto alloc : *vmem) {
    if (!alloc->isAllocated()) {
      continue;
    }

    rx::print(stderr, "  {:012x}-{:012x}: {:08x}", alloc.beginAddress(),
              alloc.endAddress(),
              alloc->flags & orbis::vmem::BlockFlags::FlexibleMemory
                  ? 0
                  : alloc->deviceOffset);

    rx::print(
        stderr, " {}{}{}{}{}",
        "_P"[alloc->flags.any_of(orbis::vmem::BlockFlags::PooledMemory)],
        "_D"[alloc->flags.any_of(orbis::vmem::BlockFlags::DirectMemory)],
        "_F"[alloc->flags.any_of(orbis::vmem::BlockFlags::FlexibleMemory)],
        "_S"[alloc->flags.any_of(orbis::vmem::BlockFlags::Stack)],
        "_C"[alloc->flags.any_of(orbis::vmem::BlockFlags::Commited)]);

    rx::print(stderr, " {}{}{} {}{}",
              "_R"[alloc->prot.any_of(orbis::vmem::Protection::CpuRead)],
              "_W"[alloc->prot.any_of(orbis::vmem::Protection::CpuWrite)],
              "_X"[alloc->prot.any_of(orbis::vmem::Protection::CpuExec)],
              "_R"[alloc->prot.any_of(orbis::vmem::Protection::GpuRead)],
              "_W"[alloc->prot.any_of(orbis::vmem::Protection::GpuWrite)]);

    rx::print(
        stderr, " {}{}{}{}{} {}",
        "_A"[alloc->flagsEx.any_of(orbis::vmem::BlockFlagsEx::Allocated)],
        "_P"[alloc->flagsEx.any_of(orbis::vmem::BlockFlagsEx::Private)],
        "_S"[alloc->flagsEx.any_of(orbis::vmem::BlockFlagsEx::Shared)],
        "_C"[alloc->flagsEx.any_of(orbis::vmem::BlockFlagsEx::PoolControl)],
        "_R"[alloc->flagsEx.any_of(orbis::vmem::BlockFlagsEx::Reserved)],
        alloc->type);

    rx::println(stderr, " {}", alloc->name);
  }
}

static orbis::ErrorCode validateOverwrite(decltype(g_vmInstance)::type *vmem,
                                          rx::AddressRange range,
                                          bool isUnmap) {
  for (auto it = vmem->lowerBound(range.beginAddress());
       it != vmem->end() && it.beginAddress() < range.endAddress(); ++it) {
    if (!it->isAllocated()) {
      continue;
    }

    if (it->flagsEx & orbis::vmem::BlockFlagsEx::Reserved) {
      return orbis::ErrorCode::ACCES;
    }

    if (it->flags & orbis::vmem::BlockFlags::PooledMemory) {
      if (!isUnmap) {
        return orbis::ErrorCode::ACCES;
      }

      while (!(it->flagsEx & orbis::vmem::BlockFlagsEx::PoolControl)) {
        ++it;

        if (it == vmem->end()) {
          return orbis::ErrorCode::ACCES;
        }

        if (!it->isAllocated() ||
            !(it->flags & orbis::vmem::BlockFlags::PooledMemory)) {
          --it;
          break;
        }
      }

      if ((it->flagsEx & orbis::vmem::BlockFlagsEx::PoolControl) &&
          it->deviceOffset != 0) {
        // control block has allocations, verify that whole reserved range going
        // to be unmapped

        auto reservedRange = rx::AddressRange::fromBeginEnd(
            it.beginAddress() - it->deviceOffset, it.endAddress());

        if (!range.contains(reservedRange)) {
          return orbis::ErrorCode::ACCES;
        }
      }
    }
  }

  return {};
}

static void release(orbis::Process *process, decltype(g_vmInstance)::type *vmem,
                    orbis::Budget *budget, rx::AddressRange range) {
  for (auto it = vmem->lowerBound(range.beginAddress());
       it != vmem->end() && it.beginAddress() < range.endAddress(); ++it) {
    if (!it->isAllocated()) {
      continue;
    }

    auto blockRange = range.intersection(it.range());

    if (it->flags & orbis::vmem::BlockFlags::FlexibleMemory) {
      if (it->device == nullptr) {
        orbis::fmem::deallocate(blockRange);
      }
      budget->release(orbis::BudgetResource::Fmem, blockRange.size());
    }

    if (it->flags & orbis::vmem::BlockFlags::DirectMemory) {
      budget->release(orbis::BudgetResource::Dmem, blockRange.size());
    }

    if (it->flags & orbis::vmem::BlockFlags::PooledMemory) {
      if (it->flags & orbis::vmem::BlockFlags::Commited) {
        orbis::blockpool::decommit(process, it.range());
      } else if (it->flagsEx & orbis::vmem::BlockFlagsEx::PoolControl) {
        orbis::blockpool::releaseControlBlock();
      }
    }

    if (orbis::vmem::toGpuProtection(it->prot) &&
        (it->flags & orbis::vmem::BlockFlags::DirectMemory |
         orbis::vmem::BlockFlags::PooledMemory)) {
      amdgpu::unmapMemory(process->pid, blockRange);
    }
  }
}

static orbis::ErrorCode validateRange(
    decltype(g_vmInstance)::type *vmem,
    decltype(g_vmInstance)::type::iterator it, rx::AddressRange range,
    rx::FunctionRef<orbis::ErrorCode(const VirtualMemoryAllocation &)> cb) {
  while (it != vmem->end() && it.beginAddress() < range.endAddress()) {
    if (auto errc = cb(it.get()); errc != orbis::ErrorCode{}) {
      return errc;
    }

    ++it;
  }

  return {};
}

static void modifyRange(
    decltype(g_vmInstance)::type *vmem,
    decltype(g_vmInstance)::type::iterator it, rx::AddressRange range,
    rx::FunctionRef<void(VirtualMemoryAllocation &, rx::AddressRange)> cb) {
  while (it != vmem->end() && it.beginAddress() < range.endAddress()) {
    auto mapRange = range.intersection(it.range());
    auto allocInfo = it.get();

    if (allocInfo.device != nullptr &&
        !(allocInfo.flags & orbis::vmem::BlockFlags::PooledMemory)) {
      allocInfo.deviceOffset += mapRange.beginAddress() - it.beginAddress();
    }

    cb(allocInfo, mapRange);
    vmem->map(mapRange.beginAddress(), mapRange.size(), allocInfo,
              orbis::AllocationFlags::Fixed, orbis::vmem::kPageSize);

    ++it;
  }
}

void orbis::vmem::initialize(Process *process, bool force) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  // FIXME: for PS5 should be extended range
  auto range = rx::AddressRange::fromBeginEnd(0x400000, 0x7000'0000'0000);
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

std::pair<rx::AddressRange, orbis::ErrorCode> orbis::vmem::reserve(
    Process *process, std::uint64_t addressHint, std::uint64_t size,
    rx::EnumBitSet<AllocationFlags> allocFlags,
    rx::EnumBitSet<BlockFlagsEx> blockFlagsEx, std::uint64_t alignment) {
  VirtualMemoryAllocation allocationInfo;
  allocationInfo.flagsEx = blockFlagsEx | BlockFlagsEx::Allocated;

  auto vmem = process->get(g_vmInstance);
  std::lock_guard lock(*vmem);

  auto [_, errc, range] =
      vmem->map(addressHint, size, allocationInfo, allocFlags, alignment);

  if (errc != std::errc{}) {
    return {{}, toErrorCode(errc)};
  }

  return {range, {}};
}

std::pair<rx::AddressRange, orbis::ErrorCode> orbis::vmem::mapFile(
    Process *process, std::uint64_t addressHint, std::uint64_t size,
    rx::EnumBitSet<AllocationFlags> allocFlags, rx::EnumBitSet<Protection> prot,
    rx::EnumBitSet<BlockFlags> blockFlags,
    rx::EnumBitSet<BlockFlagsEx> blockFlagsEx, File *file,
    std::uint64_t fileOffset, std::string_view name, std::uint64_t alignment,
    MemoryType type) {
  blockFlags |= file->device->blockFlags;

  if (blockFlags & BlockFlags::PooledMemory) {
    if (size < dmem::kPageSize * 2 || size % dmem::kPageSize) {
      return {{}, orbis::ErrorCode::INVAL};
    }
  }

  if (blockFlags & (BlockFlags::DirectMemory | BlockFlags::PooledMemory)) {
    if (prot & vmem::Protection::CpuExec) {
      return {{}, ErrorCode::ACCES};
    }

    if (alignment < dmem::kPageSize) {
      alignment = dmem::kPageSize;
    }
  }

  if (allocFlags & AllocationFlags::Fixed) {
    if (addressHint % alignment) {
      return {{}, orbis::ErrorCode::INVAL};
    }
  }

  bool canOverwrite = (allocFlags & AllocationFlags::Fixed) &&
                      !(allocFlags & AllocationFlags::NoOverwrite);

  VirtualMemoryAllocation allocationInfo;
  allocationInfo.flagsEx = blockFlagsEx | BlockFlagsEx::Allocated;
  allocationInfo.device = file->device;
  allocationInfo.prot = prot;
  allocationInfo.deviceOffset = fileOffset;
  allocationInfo.setName(process, name);

  auto vmem = process->get(g_vmInstance);
  std::lock_guard lock(*vmem);

  auto [_, errc, range] =
      vmem->map(addressHint, size, allocationInfo,
                allocFlags | AllocationFlags::Dry, alignment);

  if (errc != std::errc{}) {
    if (errc == std::errc::not_enough_memory) {
      // virtual memory shouldn't care about physical memory
      return {{}, ErrorCode::INVAL};
    }

    if (errc == std::errc::file_exists) {
      return {{}, ErrorCode::NOMEM};
    }

    return {{}, toErrorCode(errc)};
  }

  if (canOverwrite) {
    if (auto errc = validateOverwrite(vmem, range, false);
        errc != ErrorCode{}) {
      return {{}, errc};
    }
  }

  auto budget = process->getBudget();

  if (blockFlags & BlockFlags::PooledMemory) {
    prot = {};
    allocationInfo.prot = prot;
    blockFlags &= ~BlockFlags::Commited;
  }

  if (blockFlags & BlockFlags::FlexibleMemory) {
    if (!budget->acquire(BudgetResource::Fmem, size)) {
      return {{}, ErrorCode::INVAL};
    }

    if (prot) {
      blockFlags |= BlockFlags::Commited;
    }
  }

  allocFlags = AllocationFlags::Fixed | (allocFlags & AllocationFlags::NoMerge);

  if (blockFlags & BlockFlags::DirectMemory) {
    if (!budget->acquire(BudgetResource::Dmem, size)) {
      return {{}, ErrorCode::INVAL};
    }

    allocFlags |= AllocationFlags::NoMerge;

    if (prot) {
      blockFlags |= BlockFlags::Commited;
    }
  }

  if (blockFlags & BlockFlags::PooledMemory) {
    if (auto errc = blockpool::allocateControlBlock();
        errc != orbis::ErrorCode{}) {
      return {{}, errc};
    }
    allocationInfo.flagsEx |= BlockFlagsEx::PoolControl;
  }

  if (auto error = file->device->map(range, fileOffset, prot, file, process);
      error != ErrorCode{}) {
    if (blockFlags & BlockFlags::FlexibleMemory) {
      budget->release(BudgetResource::Fmem, size);
    }

    if (blockFlags & BlockFlags::DirectMemory) {
      budget->release(BudgetResource::Dmem, size);
    }

    if (blockFlags & BlockFlags::PooledMemory) {
      blockpool::releaseControlBlock();
    }

    return {{}, error};
  }

  if (canOverwrite) {
    release(process, vmem, budget, range);
  }

  allocationInfo.flags = blockFlags;

  if (type == MemoryType::Invalid) {
    if (blockFlags & BlockFlags::FlexibleMemory) {
      type = MemoryType::WbOnion;
    } else if (blockFlags & BlockFlags::DirectMemory) {
      auto queryResult = dmem::query(0, fileOffset);
      if (queryResult) {
        type = queryResult->memoryType;
      } else {
        type = MemoryType::WbOnion;
      }
    }
    allocationInfo.type = type;
  } else {
    if (blockFlags & BlockFlags::DirectMemory) {
      if (auto errc = dmem::setType(
              0, rx::AddressRange::fromBeginSize(fileOffset, size), type);
          errc != ErrorCode{}) {
        rx::die("mapDirect: failed to set dmem type");
      }
    }
  }

  {
    auto [_it, errc, _range] = vmem->map(range.beginAddress(), range.size(),
                                         allocationInfo, allocFlags, alignment);

    rx::dieIf(errc != std::errc{}, "failed to commit virtual memory {}", errc);
  }

  // vmemDump(process, rx::format("mapped {:x}-{:x} {}", range.beginAddress(),
  //                              range.endAddress(), prot));

  return {range, {}};
}

std::pair<rx::AddressRange, orbis::ErrorCode> orbis::vmem::mapDirect(
    Process *process, std::uint64_t addressHint, rx::AddressRange directRange,
    rx::EnumBitSet<Protection> prot, rx::EnumBitSet<AllocationFlags> allocFlags,
    std::string_view name, std::uint64_t alignment, MemoryType type) {
  ScopedBudgetAcquire dmemResource(process->getBudget(), BudgetResource::Dmem,
                                   directRange.size());
  if (!dmemResource) {
    return {{}, ErrorCode::INVAL};
  }

  VirtualMemoryAllocation allocationInfo;
  allocationInfo.flags = orbis::vmem::BlockFlags::DirectMemory;
  allocationInfo.flagsEx = BlockFlagsEx::Allocated;
  allocationInfo.prot = prot;
  allocationInfo.device = g_context->dmem->device;
  allocationInfo.deviceOffset = directRange.beginAddress();
  allocationInfo.type = type;
  allocationInfo.setName(process, name);

  if (prot) {
    allocationInfo.flags |= orbis::vmem::BlockFlags::Commited;
  }

  bool canOverwrite = (allocFlags & AllocationFlags::Fixed) &&
                      !(allocFlags & AllocationFlags::NoOverwrite);

  auto vmem = process->get(g_vmInstance);
  std::lock_guard lock(*vmem);

  rx::AddressRange range;

  {
    auto [_, errc, vmemRange] =
        vmem->map(addressHint, directRange.size(), allocationInfo,
                  allocFlags | AllocationFlags::Dry, alignment);
    if (errc != std::errc{}) {
      if (errc == std::errc::file_exists) {
        return {{}, ErrorCode::NOMEM};
      }

      return {{}, toErrorCode(errc)};
    }

    range = vmemRange;
  }

  if (canOverwrite) {
    if (auto errc = validateOverwrite(vmem, range, false);
        errc != orbis::ErrorCode{}) {
      return {{}, errc};
    }
  }

  if (auto errc = dmem::map(0, range, directRange.beginAddress(), prot);
      errc != orbis::ErrorCode{}) {
    return {{}, errc};
  }

  if (canOverwrite) {
    release(process, vmem, process->getBudget(), range);
  }

  if (type == MemoryType::Invalid) {
    auto queryResult = dmem::query(0, directRange.beginAddress());
    rx::dieIf(!queryResult, "mapDirect: failed to query dmem type");
    type = queryResult->memoryType;
    allocationInfo.type = type;
  } else {
    if (auto errc = dmem::setType(0, directRange, type); errc != ErrorCode{}) {
      rx::die("mapDirect: failed to set dmem type");
    }
  }

  vmem->map(range.beginAddress(), range.size(), allocationInfo,
            AllocationFlags::Fixed, alignment);

  dmemResource.commit();

  auto [pmemOffset, errc] = dmem::getPmemOffset(0, directRange.beginAddress());
  rx::dieIf(errc != ErrorCode{},
            "mapDirect: failed to query physical offset {}", errc);

  amdgpu::mapMemory(process->pid, range, type, prot, pmemOffset);

  vmemDump(process, rx::format("mapped dmem {:x}-{:x}", range.beginAddress(),
                               range.endAddress()));

  return {range, {}};
}

std::pair<rx::AddressRange, orbis::ErrorCode>
orbis::vmem::mapFlex(Process *process, std::uint64_t size,
                     rx::EnumBitSet<Protection> prot, std::uint64_t addressHint,
                     rx::EnumBitSet<AllocationFlags> allocFlags,
                     rx::EnumBitSet<BlockFlags> blockFlags,
                     std::string_view name, std::uint64_t alignment) {
  ScopedBudgetAcquire fmemResource(process->getBudget(), BudgetResource::Fmem,
                                   size);
  if (!fmemResource) {
    return {{}, ErrorCode::INVAL};
  }

  bool canOverwrite = (allocFlags & AllocationFlags::Fixed) &&
                      !(allocFlags & AllocationFlags::NoOverwrite);

  VirtualMemoryAllocation allocationInfo;
  allocationInfo.flags = orbis::vmem::BlockFlags::FlexibleMemory | blockFlags;
  allocationInfo.flagsEx = BlockFlagsEx::Allocated;
  allocationInfo.prot = prot;
  allocationInfo.type = MemoryType::WbOnion;
  allocationInfo.setName(process, name);

  if (prot) {
    allocationInfo.flags |= orbis::vmem::BlockFlags::Commited;
  }

  auto vmem = process->get(g_vmInstance);
  std::lock_guard lock(*vmem);

  rx::AddressRange vmemRange;

  {
    auto [_, errc, range] =
        vmem->map(addressHint, size, allocationInfo,
                  allocFlags | AllocationFlags::Dry, alignment);
    if (errc != std::errc{}) {
      if (errc == std::errc::file_exists) {
        return {{}, ErrorCode::NOMEM};
      }

      return {{}, toErrorCode(errc)};
    }

    vmemRange = range;
  }

  if (canOverwrite) {
    if (auto errc = validateOverwrite(vmem, vmemRange, false);
        errc != orbis::ErrorCode{}) {
      return {{}, errc};
    }
  }

  rx::AddressRange flexRange;

  {
    auto [range, errc] = fmem::allocate(size);

    if (errc != orbis::ErrorCode{}) {
      return {{}, errc};
    }

    flexRange = range;
  }

  allocationInfo.deviceOffset = flexRange.beginAddress();

  if (auto errc =
          pmem::map(vmemRange.beginAddress(), flexRange, toCpuProtection(prot));
      errc != orbis::ErrorCode{}) {
    fmem::deallocate(flexRange);
    return {{}, errc};
  }

  if (canOverwrite) {
    release(process, vmem, process->getBudget(), vmemRange);
  }

  auto [it, _errc, _range] = vmem->map(
      vmemRange.beginAddress(), vmemRange.size(), allocationInfo,
      AllocationFlags::Fixed | (allocFlags & AllocationFlags::NoMerge),
      alignment);

  // vmemDump(process, rx::format("mapFlex {:x}-{:x}", vmemRange.beginAddress(),
  //                              vmemRange.endAddress()));
  fmemResource.commit();
  return {vmemRange, {}};
}

std::pair<rx::AddressRange, orbis::ErrorCode>
orbis::vmem::commitPooled(Process *process, rx::AddressRange range,
                          MemoryType type, rx::EnumBitSet<Protection> prot) {
  VirtualMemoryAllocation allocationInfo;
  allocationInfo.flags = BlockFlags::PooledMemory | BlockFlags::Commited;
  allocationInfo.flagsEx = BlockFlagsEx::Allocated;
  allocationInfo.prot = prot;
  allocationInfo.type = type;

  auto vmem = process->get(g_vmInstance);
  std::lock_guard lock(*vmem);

  auto it = vmem->query(range.beginAddress());

  if (it == vmem->end() || !it->isAllocated() ||
      it->flags != BlockFlags::PooledMemory || !it.range().contains(range)) {
    return {{}, ErrorCode::INVAL};
  }

  allocationInfo.name = it->name;

  auto controlBlockIt = it;

  while (controlBlockIt != vmem->end() && controlBlockIt->isAllocated() &&
         (controlBlockIt->flags & BlockFlags::PooledMemory)) {
    if (!controlBlockIt->prot &&
        controlBlockIt->flags == BlockFlags::PooledMemory &&
        (controlBlockIt->flagsEx & orbis::vmem::BlockFlagsEx::PoolControl)) {
      break;
    }

    ++controlBlockIt;
  }

  if (controlBlockIt != vmem->end() &&
      controlBlockIt->flags != BlockFlags::PooledMemory) {
    controlBlockIt = vmem->end();
  }

  if (controlBlockIt == vmem->end()) {
    auto rangeWithoutCtrl = it.range();
    rangeWithoutCtrl = rx::AddressRange::fromBeginEnd(
        rangeWithoutCtrl.beginAddress(),
        rangeWithoutCtrl.endAddress() - dmem::kPageSize);

    if (!rangeWithoutCtrl.contains(range)) {
      return {{}, ErrorCode::INVAL};
    }

    if (auto errc = blockpool::commit(process, range, type, prot);
        errc != orbis::ErrorCode{}) {
      return {{}, errc};
    }

    auto controlAllocationInfo = it.get();
    controlAllocationInfo.flags = BlockFlags::PooledMemory;
    controlAllocationInfo.flagsEx =
        BlockFlagsEx::Allocated | BlockFlagsEx::PoolControl;
    controlAllocationInfo.deviceOffset = range.endAddress() - it.beginAddress();
    controlAllocationInfo.type = MemoryType::WbOnion;

    vmem->map(range.endAddress(), it.endAddress() - range.endAddress(),
              controlAllocationInfo, AllocationFlags::Fixed, kPageSize);
  } else {
    if (auto errc = blockpool::commit(process, range, type, prot);
        errc != orbis::ErrorCode{}) {
      return {{}, errc};
    }
  }

  vmem->map(range.beginAddress(), range.size(), allocationInfo,
            AllocationFlags::Fixed, kPageSize);

  // vmemDump(process, rx::format("commitPooled {:x}-{:x}",
  // range.beginAddress(),
  //                              range.endAddress()));
  return {range, {}};
}

orbis::ErrorCode orbis::vmem::decommitPooled(Process *process,
                                             rx::AddressRange range) {
  auto vmem = process->get(g_vmInstance);
  std::lock_guard lock(*vmem);

  auto it = vmem->query(range.beginAddress());

  while (it != vmem->end() && it.beginAddress() < range.endAddress()) {
    if (!it->isAllocated() ||
        it->flags != (BlockFlags::PooledMemory | BlockFlags::Commited)) {
      if (it->flagsEx & BlockFlagsEx::PoolControl) {
        if (it.range().contains(range)) {
          // ignore decommits of control block
          return {};
        }

        if (it.endAddress() <= range.endAddress()) {
          // if requested range overlaps with control block, adjust range to
          // ignore control block
          range = rx::AddressRange::fromBeginEnd(range.beginAddress(),
                                                 it.beginAddress());
          --it;
          break;
        }
      }

      return ErrorCode::INVAL;
    }

    ++it;
  }

  if (it == vmem->end()) {
    return ErrorCode::INVAL;
  }

  blockpool::decommit(process, range);
  auto allocationInfo = it.get();
  allocationInfo.prot = {};
  allocationInfo.flags = BlockFlags::PooledMemory;
  allocationInfo.flagsEx = BlockFlagsEx::Allocated;
  allocationInfo.deviceOffset = 0;
  auto unmapped = vmem->map(range.beginAddress(), range.size(), allocationInfo,
                            AllocationFlags::Fixed, kPageSize);

  auto controlBlockIt = unmapped.it;
  ++controlBlockIt;

  if (controlBlockIt != vmem->end() &&
      controlBlockIt->flags == BlockFlags::PooledMemory &&
      controlBlockIt->flagsEx & BlockFlagsEx::PoolControl) {
    allocationInfo.flagsEx |= BlockFlagsEx::PoolControl;
    // compress control block

    if (controlBlockIt.beginAddress() - controlBlockIt->deviceOffset !=
        unmapped.it.beginAddress()) {
      // if not whole reserved space was decommited, preserve offset
      allocationInfo.deviceOffset = controlBlockIt->deviceOffset;
    } else {
      allocationInfo.deviceOffset = 0;
    }

    auto controlRange = unmapped.it.range().merge(controlBlockIt.range());

    vmem->map(controlRange.beginAddress(), controlRange.size(), allocationInfo,
              AllocationFlags::Fixed, kPageSize);
  }

  // vmemDump(process, rx::format("decommitPooled {:x}-{:x}",
  // range.beginAddress(),
  //                              range.endAddress()));

  return {};
}

orbis::ErrorCode orbis::vmem::protect(Process *process, rx::AddressRange range,
                                      rx::EnumBitSet<Protection> prot) {
  auto vmem = process->get(g_vmInstance);

  range = rx::AddressRange::fromBeginEnd(
      rx::alignDown(range.beginAddress(), kPageSize),
      rx::alignUp(range.endAddress(), kPageSize));
  {
    std::lock_guard lock(*vmem);
    auto it = vmem->query(range.beginAddress());

    if (it == vmem->end()) {
      rx::println(stderr,
                  "vmem: attempt to set protection of invalid address range: "
                  "{:x}-{:x}",
                  range.beginAddress(), range.endAddress());
      return orbis::ErrorCode::INVAL;
    }

    auto errc = validateRange(vmem, it, range,
                              [](const VirtualMemoryAllocation &alloc) {
                                if (alloc.flags == BlockFlags::PooledMemory) {
                                  return ErrorCode::ACCES;
                                }

                                if (alloc.flagsEx & BlockFlagsEx::Reserved) {
                                  return ErrorCode::ACCES;
                                }

                                return ErrorCode{};
                              });

    if (errc != ErrorCode{}) {
      return errc;
    }

    if (auto errc =
            toErrorCode(rx::mem::protect(range, vmem::toCpuProtection(prot)));
        errc != ErrorCode{}) {
      return errc;
    }

    modifyRange(vmem, it, range,
                [prot](VirtualMemoryAllocation &alloc, rx::AddressRange) {
                  if (!alloc.isAllocated()) {
                    return;
                  }

                  if (alloc.device != nullptr &&
                      alloc.flags & BlockFlags::FlexibleMemory) {
                    alloc.prot = prot & ~Protection::CpuExec;
                  }

                  alloc.flags = alloc.prot
                                    ? alloc.flags | BlockFlags::Commited
                                    : alloc.flags & ~BlockFlags::Commited;
                });
  }

  amdgpu::protectMemory(process->pid, range, prot);

  // vmemDump(process, rx::format("protected {:x}-{:x} {}",
  // range.beginAddress(),
  //                              range.endAddress(), prot));

  return {};
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

  modifyRange(vmem, it, range,
              [name](VirtualMemoryAllocation &alloc, rx::AddressRange) {
                if (alloc.isAllocated()) {
                  alloc.name = name;
                }
              });

  return {};
}

orbis::ErrorCode orbis::vmem::setType(Process *process, rx::AddressRange range,
                                      MemoryType type) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);
  auto it = vmem->query(range.beginAddress());

  if (it == vmem->end()) {
    return orbis::ErrorCode::INVAL;
  }

  modifyRange(
      vmem, it, range,
      [type](VirtualMemoryAllocation &alloc, rx::AddressRange range) {
        if (!alloc.isAllocated()) {
          return;
        }

        if (alloc.flags & BlockFlags::DirectMemory) {
          dmem::setType(
              0,
              rx::AddressRange::fromBeginSize(alloc.deviceOffset, range.size()),
              type);
          alloc.type = type;
          return;
        }

        if (alloc.flags != (BlockFlags::PooledMemory | BlockFlags::Commited)) {
          alloc.type = type;
          return;
        }
      });

  return {};
}

orbis::ErrorCode
orbis::vmem::setTypeAndProtect(Process *process, rx::AddressRange range,
                               MemoryType type,
                               rx::EnumBitSet<Protection> prot) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);
  auto it = vmem->query(range.beginAddress());

  if (it == vmem->end()) {
    return orbis::ErrorCode::INVAL;
  }

  auto errc =
      validateRange(vmem, it, range, [](const VirtualMemoryAllocation &alloc) {
        if (alloc.flags == BlockFlags::PooledMemory) {
          return ErrorCode::ACCES;
        }

        if (alloc.flagsEx & BlockFlagsEx::Reserved) {
          return ErrorCode::ACCES;
        }

        return ErrorCode{};
      });

  if (errc != ErrorCode{}) {
    return errc;
  }

  if (auto errc =
          toErrorCode(rx::mem::protect(range, vmem::toCpuProtection(prot)));
      errc != ErrorCode{}) {
    return errc;
  }

  modifyRange(
      vmem, it, range,
      [type, prot](VirtualMemoryAllocation &alloc, rx::AddressRange range) {
        if (alloc.isAllocated()) {
          alloc.type = type;
          alloc.prot = prot;

          if (alloc.flags & BlockFlags::DirectMemory) {
            dmem::setType(0,
                          rx::AddressRange::fromBeginSize(alloc.deviceOffset,
                                                          range.size()),
                          type);
            return;
          }
        }
      });

  amdgpu::protectMemory(process->pid, range, prot);
  return {};
}

orbis::ErrorCode orbis::vmem::unmap(Process *process, rx::AddressRange range) {
  auto vmem = process->get(g_vmInstance);
  auto budget = process->getBudget();
  VirtualMemoryAllocation allocationInfo{};

  range = rx::AddressRange::fromBeginEnd(
      rx::alignDown(range.beginAddress(), kPageSize),
      rx::alignUp(range.endAddress(), kPageSize));

  orbis::ErrorCode result;
  {
    std::lock_guard lock(*vmem);

    if (auto errc = validateOverwrite(vmem, range, true);
        errc != orbis::ErrorCode{}) {
      return errc;
    }

    release(process, vmem, budget, range);

    auto [it, errc, _] =
        vmem->map(range.beginAddress(), range.size(), allocationInfo,
                  AllocationFlags::Fixed, kPageSize);

    result = toErrorCode(errc);
  }

  rx::mem::release(range, kPageSize);
  amdgpu::unmapMemory(process->pid, range);

  // vmemDump(process, rx::format("unmap {:x}-{:x}", range.beginAddress(),
  //                              range.endAddress()));
  return result;
}

std::optional<orbis::vmem::QueryResult>
orbis::vmem::query(Process *process, std::uint64_t address, bool lowerBound) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  auto it = vmem->lowerBound(address);

  constexpr auto restrictedArea =
      rx::AddressRange::fromBeginSize(0x800000000, 0x100000000);

  if (lowerBound) {
    while (it != vmem->end()) {
      if (it->isAllocated() && !restrictedArea.intersects(it.range())) {
        break;
      }

      ++it;
    }

    if (it == vmem->end()) {
      return {};
    }
  } else if (it == vmem->end() || !it.range().contains(address) ||
             !it->isAllocated() || restrictedArea.intersects(it.range())) {
    return {};
  }

  orbis::vmem::QueryResult result{};
  result.start = it.beginAddress();
  result.end = it.endAddress();

  if (!(it->flags & BlockFlags::FlexibleMemory) || it->device != nullptr) {
    result.offset = it->deviceOffset;
  }

  if (it->flags & BlockFlags::DirectMemory) {
    result.memoryType = it->type;
  } else if (it->flags == (BlockFlags::PooledMemory | BlockFlags::Commited)) {
    result.memoryType = it->type;
  }

  result.protection = it->prot;
  result.flags = it->flags;
  result.name = it->name;

  // vmemDump(process, rx::format("query result {:x}-{:x} {:x} {} {} {}",
  //                              result.start, result.end, result.offset,
  //                              result.protection, result.flags,
  //                              result.name));

  return result;
}

std::optional<orbis::vmem::MemoryProtection>
orbis::vmem::queryProtection(Process *process, std::uint64_t address,
                             bool lowerBound) {
  auto vmem = process->get(g_vmInstance);

  std::lock_guard lock(*vmem);

  auto it = vmem->lowerBound(address);
  if (it == vmem->end()) {
    return {};
  }

  if (lowerBound) {
    while (it != vmem->end() && !it->isAllocated()) {
      ++it;
    }

    if (it == vmem->end()) {
      return {};
    }
  } else if (!it.range().contains(address) || !it->isAllocated()) {
    return {};
  }

  orbis::vmem::MemoryProtection result{};
  result.startAddress = it.beginAddress();
  result.endAddress = it.endAddress();
  result.prot = it->prot;
  return result;
}
