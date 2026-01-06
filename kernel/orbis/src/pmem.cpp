#include "pmem.hpp"
#include "IoDevice.hpp"
#include "KernelAllocator.hpp"
#include "KernelObject.hpp"
#include "error.hpp"
#include "error/ErrorCode.hpp"
#include "file.hpp"
#include "kernel/KernelObject.hpp"
#include "kernel/MemoryResource.hpp"
#include "rx/AddressRange.hpp"
#include "rx/Rc.hpp"
#include "rx/mem.hpp"
#include "rx/print.hpp"
#include "vmem.hpp"
#include <cassert>
#include <rx/Mappable.hpp>

struct PhysicalMemoryAllocation {
  bool allocated = false;

  [[nodiscard]] bool isAllocated() const { return allocated; }
  [[nodiscard]] bool isRelated(const PhysicalMemoryAllocation &left,
                               rx::AddressRange, rx::AddressRange) const {
    return allocated == left.allocated;
  }

  [[nodiscard]] PhysicalMemoryAllocation
  merge(const PhysicalMemoryAllocation &other, rx::AddressRange,
        rx::AddressRange) const {
    assert(other.allocated == allocated);
    return other;
  }

  bool operator==(const PhysicalMemoryAllocation &) const = default;
};

struct PhysicalMemoryResource
    : kernel::AllocableResource<PhysicalMemoryAllocation, orbis::kallocator,
                                kernel::ExternalResource> {
  std::size_t size;
  rx::Mappable mappable;

  std::errc create(rx::Mappable mappable, std::size_t size) {
    if (size == 0 || !mappable) {
      return std::errc::invalid_argument;
    }

    if (auto errc =
            BaseResource::create(rx::AddressRange::fromBeginSize(0, size));
        errc != std::errc{}) {
      return errc;
    }

    this->size = size;
    this->mappable = std::move(mappable);
    return {};
  }
};

static auto g_pmemInstance = orbis::createGlobalObject<
    kernel::LockableKernelObject<PhysicalMemoryResource>>();

struct PhysicalMemory : orbis::IoDevice {
  orbis::File file;

  PhysicalMemory() {
    incRef(); // do not delete global object

    file.device = this;
    file.incRef(); // do not delete property
  }

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    rx::die("open PhysicalMemory device");
  }

  orbis::ErrorCode map(rx::AddressRange range, std::int64_t offset,
                       rx::EnumBitSet<orbis::vmem::Protection> protection,
                       orbis::File *, orbis::Process *) override {
    return orbis::pmem::map(
        range.beginAddress(),
        rx::AddressRange::fromBeginSize(offset, range.size()),
        orbis::vmem::toCpuProtection(protection));
  }

  void serialize(rx::Serializer &s) const {}
  void deserialize(rx::Deserializer &s) {}
};

static auto g_phyMemory = orbis::createGlobalObject<PhysicalMemory>();

orbis::ErrorCode orbis::pmem::initialize(rx::Mappable mappable,
                                         std::uint64_t size) {
  std::lock_guard lock(*g_pmemInstance);
  rx::println("pmem: {:x}", size);

  return toErrorCode(g_pmemInstance->create(std::move(mappable), size));
}

void orbis::pmem::destroy() {
  std::lock_guard lock(*g_pmemInstance);
  g_pmemInstance->destroy();
}

std::pair<rx::AddressRange, orbis::ErrorCode>
orbis::pmem::allocate(std::uint64_t addressHint, std::uint64_t size,
                      rx::EnumBitSet<AllocationFlags> flags,
                      std::uint64_t alignment) {
  std::lock_guard lock(*g_pmemInstance);
  PhysicalMemoryAllocation allocation{.allocated = true};
  auto [it, errc, range] =
      g_pmemInstance->map(addressHint, size, allocation, flags, alignment);

  if (errc != std::errc{}) {
    return {{}, toErrorCode(errc)};
  }

  return {range, {}};
}

orbis::ErrorCode orbis::pmem::deallocate(rx::AddressRange range) {
  std::lock_guard lock(*g_pmemInstance);
  PhysicalMemoryAllocation allocation{};
  auto [it, errc, _] =
      g_pmemInstance->map(range.beginAddress(), range.size(), allocation,
                          AllocationFlags::Fixed, 1);

  return toErrorCode(errc);
}

std::optional<rx::AddressRange> orbis::pmem::query(std::uint64_t address) {
  std::lock_guard lock(*g_pmemInstance);
  auto result = g_pmemInstance->query(address);

  if (result == g_pmemInstance->end()) {
    return {};
  }

  return result.range();
}

orbis::ErrorCode
orbis::pmem::map(std::uint64_t virtualAddress, rx::AddressRange range,
                 rx::EnumBitSet<rx::mem::Protection> protection) {
  auto virtualRange =
      rx::AddressRange::fromBeginSize(virtualAddress, range.size());
  auto errc = g_pmemInstance->mappable.map(virtualRange, range.beginAddress(),
                                           protection, vmem::kPageSize);

  return toErrorCode(errc);
}

void *orbis::pmem::mapInternal(rx::AddressRange range,
                               rx::EnumBitSet<rx::mem::Protection> protection) {
  auto [pointer, errc] = g_pmemInstance->mappable.map(
      range.size(), range.beginAddress(), protection);
  if (errc != std::errc{}) {
    return nullptr;
  }

  return pointer;
}

void orbis::pmem::unmapInternal(void *data, std::size_t size) {
  auto address = std::bit_cast<uintptr_t>(data);
  rx::mem::release(rx::AddressRange::fromBeginSize(address, size), 0);
}

std::size_t orbis::pmem::getSize() { return g_pmemInstance->size; }
orbis::IoDevice *orbis::pmem::getDevice() { return g_phyMemory.get(); }
orbis::File *orbis::pmem::getFile() { return &g_phyMemory->file; }
