#include "pmem.hpp"
#include "IoDevice.hpp"
#include "KernelObject.hpp"
#include "error.hpp"
#include "error/ErrorCode.hpp"
#include "kernel/KernelObject.hpp"
#include "kernel/MemoryResource.hpp"
#include "rx/AddressRange.hpp"
#include "vmem.hpp"
#include <cassert>
#include <rx/Mappable.hpp>

struct PhysicalMemoryAllocation {
  orbis::pmem::MemoryType type = orbis::pmem::MemoryType::Invalid;

  [[nodiscard]] bool isAllocated() const {
    return type != orbis::pmem::MemoryType::Invalid;
  }
  [[nodiscard]] bool isRelated(const PhysicalMemoryAllocation &left,
                               rx::AddressRange, rx::AddressRange) const {
    return type == left.type;
  }

  [[nodiscard]] PhysicalMemoryAllocation
  merge(const PhysicalMemoryAllocation &other, rx::AddressRange,
        rx::AddressRange) const {
    assert(other.type == type);
    return other;
  }

  bool operator==(const PhysicalMemoryAllocation &) const = default;
};

using MappableMemoryResource =
    kernel::MappableResource<decltype([](std::size_t size) {
      return rx::Mappable::CreateMemory(size);
    })>;

using PhysicalMemoryResource =
    kernel::AllocableResource<PhysicalMemoryAllocation, MappableMemoryResource>;

static auto g_pmemInstance = orbis::createGlobalObject<
    kernel::LockableKernelObject<PhysicalMemoryResource>>();

struct PhysicalMemory : orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    rx::die("open PhysicalMemory device");
  }

  orbis::ErrorCode map(rx::AddressRange range, std::int64_t offset,
                       rx::EnumBitSet<rx::mem::Protection> protection,
                       orbis::Process *) override {
    return orbis::pmem::map(
        range.beginAddress(),
        rx::AddressRange::fromBeginSize(offset, range.size()), protection);
  }

  void serialize(rx::Serializer &s) const {}
  void deserialize(rx::Deserializer &s) {}
};

static auto g_phyMemory = orbis::createGlobalObject<PhysicalMemory>();

orbis::ErrorCode orbis::pmem::initialize(std::uint64_t size) {
  std::lock_guard lock(*g_pmemInstance);
  return toErrorCode(
      g_pmemInstance->create(rx::AddressRange::fromBeginSize(0, size)));
}

void orbis::pmem::destroy() {
  std::lock_guard lock(*g_pmemInstance);
  g_pmemInstance->destroy();
}

std::pair<rx::AddressRange, orbis::ErrorCode> orbis::pmem::allocate(
    std::uint64_t addressHint, std::uint64_t size, MemoryType memoryType,
    rx::EnumBitSet<AllocationFlags> flags, std::uint64_t alignment) {
  std::lock_guard lock(*g_pmemInstance);
  PhysicalMemoryAllocation allocation{.type = memoryType};
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

std::optional<orbis::pmem::AllocatedMemory>
orbis::pmem::query(std::uint64_t address) {
  std::lock_guard lock(*g_pmemInstance);
  auto result = g_pmemInstance->query(address);

  if (result == g_pmemInstance->end()) {
    return {};
  }

  return AllocatedMemory{.range = result.range(), .memoryType = result->type};
}

orbis::ErrorCode
orbis::pmem::map(std::uint64_t virtualAddress, rx::AddressRange range,
                 rx::EnumBitSet<rx::mem::Protection> protection) {
  auto virtualRange =
      rx::AddressRange::fromBeginSize(virtualAddress, range.size());
  auto errc = g_pmemInstance->mappable.map(virtualRange, range.beginAddress(),
                                           protection, orbis::vmem::kPageSize);

  return toErrorCode(errc);
}

std::size_t orbis::pmem::getSize() { return g_pmemInstance->size; }
orbis::IoDevice *orbis::pmem::getDevice() { return g_phyMemory.get(); }

