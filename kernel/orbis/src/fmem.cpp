#include "fmem.hpp"

#include "KernelAllocator.hpp"
#include "KernelObject.hpp"
#include "error.hpp"
#include "kernel/KernelObject.hpp"
#include "kernel/MemoryResource.hpp"
#include "pmem.hpp"
#include "rx/AddressRange.hpp"
#include "rx/debug.hpp"
#include "rx/print.hpp"
#include "vmem.hpp"
#include <cassert>
#include <rx/Mappable.hpp>

struct FlexibleMemoryAllocation {
  bool allocated = false;

  [[nodiscard]] bool isAllocated() const { return allocated; }
  [[nodiscard]] bool isRelated(const FlexibleMemoryAllocation &,
                               rx::AddressRange, rx::AddressRange) const {
    return true;
  }

  [[nodiscard]] FlexibleMemoryAllocation
  merge(const FlexibleMemoryAllocation &other, rx::AddressRange,
        rx::AddressRange) const {
    return other;
  }

  bool operator==(const FlexibleMemoryAllocation &) const = default;
};

using FlexibleMemoryResource =
    kernel::AllocableResource<FlexibleMemoryAllocation, orbis::kallocator>;

static auto g_fmemInstance = orbis::createGlobalObject<
    kernel::LockableKernelObject<FlexibleMemoryResource>>();

orbis::ErrorCode orbis::fmem::initialize(std::uint64_t size) {
  auto [range, errc] =
      pmem::allocate(0, size, kernel::AllocationFlags::Stack, vmem::kPageSize);
  if (errc != ErrorCode{}) {
    return errc;
  }

  rx::println("fmem: {:x}-{:x}", range.beginAddress(), range.endAddress());

  std::lock_guard lock(*g_fmemInstance);
  return toErrorCode(g_fmemInstance->create(range));
}

void orbis::fmem::destroy() {
  std::lock_guard lock(*g_fmemInstance);

  for (auto allocation : g_fmemInstance->allocations) {
    pmem::deallocate(allocation);
  }

  g_fmemInstance->destroy();
}

std::pair<rx::AddressRange, orbis::ErrorCode>
orbis::fmem::allocate(std::uint64_t size) {
  std::lock_guard lock(*g_fmemInstance);
  FlexibleMemoryAllocation allocation{.allocated = true};
  auto [it, errc, range] = g_fmemInstance->map(
      0, size, allocation, kernel::AllocationFlags::NoMerge, vmem::kPageSize);

  if (errc != std::errc{}) {
    return {{}, toErrorCode(errc)};
  }

  return {range, {}};
}

orbis::ErrorCode orbis::fmem::deallocate(rx::AddressRange range) {
  FlexibleMemoryAllocation allocation{};
  std::lock_guard lock(*g_fmemInstance);
  auto [it, errc, _] =
      g_fmemInstance->map(range.beginAddress(), range.size(), allocation,
                          AllocationFlags::Fixed, 1);

  return toErrorCode(errc);
}
