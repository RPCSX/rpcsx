#include "fmem.hpp"

#include "KernelObject.hpp"
#include "error.hpp"
#include "kernel/KernelObject.hpp"
#include "kernel/MemoryResource.hpp"
#include "pmem.hpp"
#include "rx/AddressRange.hpp"
#include "thread/Process.hpp"
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
    kernel::AllocableResource<FlexibleMemoryAllocation>;

static auto g_fmemInstance = orbis::createProcessLocalObject<
    kernel::LockableKernelObject<FlexibleMemoryResource>>();

orbis::ErrorCode orbis::fmem::initialize(Process *process, std::uint64_t size) {
  auto [range, errc] =
      pmem::allocate(pmem::getSize() - 1, size, pmem::MemoryType::WbOnion,
                     kernel::AllocationFlags::Stack, vmem::kPageSize);
  if (errc != ErrorCode{}) {
    return errc;
  }

  auto fmem = process->get(g_fmemInstance);
  std::lock_guard lock(*fmem);
  return toErrorCode(fmem->create(range));
}

void orbis::fmem::destroy(Process *process) {
  auto fmem = process->get(g_fmemInstance);

  std::lock_guard lock(*fmem);

  for (auto allocation : fmem->allocations) {
    pmem::deallocate(allocation);
  }

  fmem->destroy();
}

std::pair<rx::AddressRange, orbis::ErrorCode>
orbis::fmem::allocate(Process *process, std::uint64_t size) {
  auto fmem = process->get(g_fmemInstance);

  std::lock_guard lock(*fmem);
  FlexibleMemoryAllocation allocation{.allocated = true};
  auto [it, errc, range] = fmem->map(
      0, size, allocation, kernel::AllocationFlags::NoMerge, vmem::kPageSize);

  if (errc != std::errc{}) {
    return {{}, toErrorCode(errc)};
  }

  return {range, {}};
}

orbis::ErrorCode orbis::fmem::deallocate(Process *process,
                                         rx::AddressRange range) {
  FlexibleMemoryAllocation allocation{};
  auto fmem = process->get(g_fmemInstance);
  std::lock_guard lock(*fmem);
  auto [it, errc, _] = fmem->map(range.beginAddress(), range.size(), allocation,
                                 AllocationFlags::Fixed, 1);

  return toErrorCode(errc);
}
