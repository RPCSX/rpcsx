#include "blockpool.hpp"
#include "KernelAllocator.hpp"
#include "KernelObject.hpp"
#include "dmem.hpp"
#include "pmem.hpp"
#include "rx/AddressRange.hpp"
#include "rx/MemoryTable.hpp"
#include "rx/die.hpp"
#include "rx/format.hpp"
#include "thread/Process.hpp"
#include "vmem.hpp"
#include <algorithm>
#include <iterator>
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

struct PooledMemoryResource {
  struct CommitedBlock {
    std::uint64_t pmemAddress;
    orbis::MemoryType type;
    bool operator==(const CommitedBlock &) const = default;
  };

  std::uint64_t total = 0;
  std::uint64_t used = 0;
  orbis::kvector<rx::AddressRange> freeBlocks;
  rx::MemoryTableWithPayload<CommitedBlock, orbis::kallocator> usedBlocks;
  orbis::kvector<std::uint64_t> reservedPages;

  void clear() {
    total = 0;
    used = 0;
    freeBlocks.clear();
    usedBlocks.clear();
    reservedPages.clear();
  }

  void addFreeBlock(rx::AddressRange dmemRange) {
    if (freeBlocks.empty()) {
      freeBlocks.push_back(dmemRange);
      return;
    }

    auto it = std::upper_bound(
        freeBlocks.begin(), freeBlocks.end(), dmemRange.beginAddress(),
        [](auto lhs, auto rhs) {
          if constexpr (requires { lhs.beginAddress() < rhs; }) {
            return lhs.beginAddress() < rhs;
          } else {
            return lhs < rhs.beginAddress();
          }
        });

    if (it != freeBlocks.end() &&
        dmemRange.endAddress() == it->beginAddress()) {
      *it = rx::AddressRange::fromBeginEnd(it->beginAddress(),
                                           dmemRange.endAddress());
      return;
    }

    if (it != freeBlocks.begin()) {
      auto prev = std::prev(it);

      if (prev->endAddress() == dmemRange.beginAddress()) {
        *prev = rx::AddressRange::fromBeginEnd(prev->beginAddress(),
                                               dmemRange.endAddress());
        return;
      }
    }

    freeBlocks.insert(it, dmemRange);
    return;
  }

  void expand(rx::AddressRange dmemRange) {
    addFreeBlock(dmemRange);
    total += dmemRange.size();
  }

  orbis::ErrorCode reserve(std::size_t size) {
    if (size > total) {
      return orbis::ErrorCode::INVAL;
    }

    used += size;
    return {};
  }

  std::pair<std::uint64_t, orbis::ErrorCode> reservePage() {
    if (total - used < orbis::dmem::kPageSize) {
      return {{}, orbis::ErrorCode::INVAL};
    }

    used += orbis::dmem::kPageSize;
    auto &block = freeBlocks.back();
    auto allocatedPage = block.beginAddress();

    if (block.endAddress() == allocatedPage + orbis::dmem::kPageSize) {
      freeBlocks.pop_back();
    } else {
      block = rx::AddressRange::fromBeginEnd(
          allocatedPage + orbis::dmem::kPageSize, block.endAddress());
    }

    reservedPages.push_back(allocatedPage);
    return {allocatedPage, {}};
  }

  std::pair<std::uint64_t, orbis::ErrorCode> releasePage() {
    if (reservedPages.empty()) {
      return {{}, orbis::ErrorCode::INVAL};
    }

    used -= orbis::dmem::kPageSize;
    total -= orbis::dmem::kPageSize;

    auto address = reservedPages.back();
    reservedPages.pop_back();
    return {address, {}};
  }

  void moveTo(PooledMemoryResource &other, std::size_t size) {
    while (size > 0 && total > 0) {
      auto &block = freeBlocks.back();

      if (block.size() > size) {
        total -= size;

        auto moveRange =
            rx::AddressRange::fromBeginSize(block.beginAddress(), size);

        other.expand(moveRange);

        block = rx::AddressRange::fromBeginEnd(moveRange.endAddress(),
                                               block.endAddress());
        break;
      }

      total -= block.size();
      size -= block.size();
      other.expand(block);
      freeBlocks.pop_back();
    }
  }

  void commit(orbis::Process *process, rx::AddressRange virtualRange,
              orbis::MemoryType type,
              rx::EnumBitSet<orbis::vmem::Protection> protection) {
    while (virtualRange.isValid()) {
      auto &block = freeBlocks.back();

      if (block.size() >= virtualRange.size()) [[likely]] {
        auto mapPhysicalRange = rx::AddressRange::fromBeginSize(
            block.beginAddress(), virtualRange.size());
        auto errc =
            orbis::pmem::map(virtualRange.beginAddress(), mapPhysicalRange,
                             orbis::vmem::toCpuProtection(protection));

        rx::dieIf(errc != orbis::ErrorCode{},
                  "blockpool: failed to map physical memory");
        amdgpu::mapMemory(process->pid, virtualRange, type, protection,
                          block.beginAddress());

        if (mapPhysicalRange.endAddress() == block.endAddress()) {
          freeBlocks.pop_back();
        } else {
          block = rx::AddressRange::fromBeginEnd(mapPhysicalRange.endAddress(),
                                                 block.endAddress());
        }

        usedBlocks.map(virtualRange,
                       {.pmemAddress = block.beginAddress(), .type = type},
                       false);
        used += virtualRange.size();
        break;
      }

      auto mapVirtualRange = rx::AddressRange::fromBeginSize(
          virtualRange.beginAddress(), block.size());

      auto errc = orbis::pmem::map(mapVirtualRange.beginAddress(), block,
                                   orbis::vmem::toCpuProtection(protection));

      rx::dieIf(errc != orbis::ErrorCode{},
                "blockpool: failed to map physical memory, vmem {:x}-{:x}, "
                "pmem {:x}-{:x}, commit vmem {:x}-{:x}, error {}",
                mapVirtualRange.beginAddress(), mapVirtualRange.endAddress(),
                block.beginAddress(), block.endAddress(),
                virtualRange.beginAddress(), virtualRange.endAddress(), errc);

      amdgpu::mapMemory(process->pid, mapVirtualRange, type, protection,
                        block.beginAddress());

      usedBlocks.map(mapVirtualRange,
                     {.pmemAddress = block.beginAddress(), .type = type},
                     false);
      freeBlocks.pop_back();

      virtualRange = rx::AddressRange::fromBeginEnd(
          mapVirtualRange.endAddress(), virtualRange.endAddress());
    }
  }

  void decommit(orbis::Process *process, rx::AddressRange virtualRange) {
    auto it = usedBlocks.lowerBound(virtualRange.beginAddress());

    if (it != usedBlocks.end() &&
        it.beginAddress() < virtualRange.beginAddress()) {
      auto itRange = it.range();
      auto decommitRange = itRange.intersection(virtualRange);
      used -= decommitRange.size();

      auto decommitPmemRange = rx::AddressRange::fromBeginSize(
          it->pmemAddress +
              (virtualRange.beginAddress() - itRange.beginAddress()),
          decommitRange.size());
      addFreeBlock(decommitPmemRange);
      usedBlocks.unmap(decommitRange);
      amdgpu::unmapMemory(process->pid, decommitRange);
      ++it;
    }

    while (it != usedBlocks.end() &&
           it.beginAddress() < virtualRange.endAddress()) {
      auto itRange = it.range();
      auto decommitRange = itRange.intersection(virtualRange);
      used -= decommitRange.size();

      addFreeBlock(rx::AddressRange::fromBeginSize(it->pmemAddress,
                                                   decommitRange.size()));
      amdgpu::unmapMemory(process->pid, decommitRange);

      if (itRange == decommitRange) {
        it = usedBlocks.unmap(it);
      } else {
        usedBlocks.unmap(decommitRange);
        break;
      }
    }
  }

  std::optional<orbis::MemoryType> getMemoryType(std::uint64_t address) {
    auto it = usedBlocks.queryArea(address);
    if (it == usedBlocks.end()) {
      return {};
    }
    return it->type;
  }
};

static auto g_blockpool = orbis::createGlobalObject<
    kernel::LockableKernelObject<PooledMemoryResource>>();

static auto g_cachedBlockpool = orbis::createGlobalObject<
    kernel::LockableKernelObject<PooledMemoryResource>>();

void orbis::blockpool::clear() {
  std::scoped_lock lock(*g_blockpool, *g_cachedBlockpool);

  g_blockpool->clear();
  g_cachedBlockpool->clear();
}

orbis::ErrorCode orbis::blockpool::expand(rx::AddressRange dmemRange) {
  std::scoped_lock lock(*g_blockpool);
  g_blockpool->expand(dmemRange);
  return {};
}

orbis::ErrorCode orbis::blockpool::allocateControlBlock() {
  std::scoped_lock cachedLock(*g_cachedBlockpool);
  if (g_cachedBlockpool->used < g_cachedBlockpool->total) {
    return g_cachedBlockpool->reservePage().second;
  }

  std::scoped_lock lock(*g_blockpool);

  if (g_blockpool->total - g_blockpool->used < dmem::kPageSize) {
    return ErrorCode::INVAL;
  }

  g_blockpool->moveTo(*g_cachedBlockpool, dmem::kPageSize);
  return g_cachedBlockpool->reservePage().second;
}

orbis::ErrorCode orbis::blockpool::releaseControlBlock() {
  std::scoped_lock lock(*g_cachedBlockpool);

  // control block is always cached
  if (!g_cachedBlockpool->reservedPages.empty()) {
    auto [page, errc] = g_cachedBlockpool->releasePage();
    if (errc != ErrorCode{}) {
      return errc;
    }

    g_cachedBlockpool->expand(
        rx::AddressRange::fromBeginSize(page, dmem::kPageSize));
    return {};
  }

  return ErrorCode::INVAL;
}

orbis::ErrorCode
orbis::blockpool::commit(Process *process, rx::AddressRange vmemRange,
                         MemoryType type,
                         rx::EnumBitSet<orbis::vmem::Protection> protection) {
  auto pool = type == MemoryType::WbOnion ? g_cachedBlockpool : g_blockpool;
  auto otherPool =
      type == MemoryType::WbOnion ? g_blockpool : g_cachedBlockpool;

  std::scoped_lock lock(*pool);

  if (auto avail = pool->total - pool->used; avail < vmemRange.size()) {
    // try to steal free blocks from other pool

    std::scoped_lock lock(*otherPool);
    auto pullSize = vmemRange.size() - avail;

    if (otherPool->total - otherPool->used < pullSize) {
      return ErrorCode::NOMEM;
    }

    otherPool->moveTo(*pool, pullSize);
  }

  pool->commit(process, vmemRange, type, protection);
  return {};
}

void orbis::blockpool::decommit(Process *process, rx::AddressRange vmemRange) {
  std::scoped_lock lock(*g_cachedBlockpool, *g_blockpool);
  g_cachedBlockpool->decommit(process, vmemRange);
  g_blockpool->decommit(process, vmemRange);
}

std::optional<orbis::MemoryType>
orbis::blockpool::getType(std::uint64_t address) {
  {
    std::scoped_lock lock(*g_cachedBlockpool);

    if (auto result = g_cachedBlockpool->getMemoryType(address)) {
      return result;
    }
  }

  std::scoped_lock lock(*g_blockpool);
  return g_blockpool->getMemoryType(address);
}

orbis::blockpool::BlockStats orbis::blockpool::stats() {
  BlockStats result{};

  {
    std::scoped_lock lock(*g_cachedBlockpool, *g_blockpool);
    result.availFlushedBlocks =
        (g_blockpool->total - g_blockpool->used) / dmem::kPageSize;
    result.availCachedBlocks =
        (g_cachedBlockpool->total - g_cachedBlockpool->used) / dmem::kPageSize;
    result.commitFlushedBlocks = g_blockpool->used / dmem::kPageSize;
    result.commitCachedBlocks = g_cachedBlockpool->used / dmem::kPageSize;
  }

  return result;
}
