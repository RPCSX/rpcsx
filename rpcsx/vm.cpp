#include "vm.hpp"
#include "gpu/DeviceCtl.hpp"
#include "io-device.hpp"
#include "iodev/dmem.hpp"
#include "orbis-config.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/Rc.hpp"
#include "rx/format.hpp"
#include "rx/print.hpp"
#include "rx/watchdog.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <rx/MemoryTable.hpp>
#include <rx/align.hpp>
#include <rx/mem.hpp>
#include <sys/mman.h>
#include <unistd.h>

static std::mutex g_mtx;

std::string vm::mapFlagsToString(std::int32_t flags) {
  std::string result;

  if ((flags & kMapFlagShared) == kMapFlagShared) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Shared";
    flags &= ~kMapFlagShared;
  }
  if ((flags & kMapFlagPrivate) == kMapFlagPrivate) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Private";
    flags &= ~kMapFlagPrivate;
  }
  if ((flags & kMapFlagFixed) == kMapFlagFixed) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Fixed";
    flags &= ~kMapFlagFixed;
  }
  if ((flags & kMapFlagRename) == kMapFlagRename) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Rename";
    flags &= ~kMapFlagRename;
  }
  if ((flags & kMapFlagNoReserve) == kMapFlagNoReserve) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "NoReserve";
    flags &= ~kMapFlagNoReserve;
  }
  if ((flags & kMapFlagNoOverwrite) == kMapFlagNoOverwrite) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "NoOverwrite";
    flags &= ~kMapFlagNoOverwrite;
  }
  if ((flags & kMapFlagVoid) == kMapFlagVoid) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Void";
    flags &= ~kMapFlagVoid;
  }
  if ((flags & kMapFlagHasSemaphore) == kMapFlagHasSemaphore) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "HasSemaphore";
    flags &= ~kMapFlagHasSemaphore;
  }
  if ((flags & kMapFlagStack) == kMapFlagStack) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Stack";
    flags &= ~kMapFlagStack;
  }
  if ((flags & kMapFlagNoSync) == kMapFlagNoSync) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "NoSync";
    flags &= ~kMapFlagNoSync;
  }
  if ((flags & kMapFlagAnonymous) == kMapFlagAnonymous) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Anonymous";
    flags &= ~kMapFlagAnonymous;
  }
  if ((flags & kMapFlagSystem) == kMapFlagSystem) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "System";
    flags &= ~kMapFlagSystem;
  }
  if ((flags & kMapFlagAllAvailable) == kMapFlagAllAvailable) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "AllAvailable";
    flags &= ~kMapFlagAllAvailable;
  }
  if ((flags & kMapFlagNoCore) == kMapFlagNoCore) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "NoCore";
    flags &= ~kMapFlagNoCore;
  }
  if ((flags & kMapFlagPrefaultRead) == kMapFlagPrefaultRead) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "PrefaultRead";
    flags &= ~kMapFlagPrefaultRead;
  }
  if ((flags & kMapFlagSelf) == kMapFlagSelf) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Self";
    flags &= ~kMapFlagSelf;
  }

  auto alignment = (flags & kMapFlagsAlignMask) >> kMapFlagsAlignShift;
  flags &= ~kMapFlagsAlignMask;

  if (alignment != 0) {
    if (!result.empty()) {
      result += " | ";
    }

    result += "Alignment(" + std::to_string(alignment) + ")";
  }

  if (flags != 0) {
    if (!result.empty()) {
      result += " | ";
    }

    result += std::to_string(flags);
  }

  return result;
}

std::string vm::mapProtToString(std::int32_t prot) {
  std::string result;

  if ((prot & kMapProtCpuRead) == kMapProtCpuRead) {
    if (!result.empty()) {
      result += " | ";
    }
    result += "CpuRead";
    prot &= ~kMapProtCpuRead;
  }
  if ((prot & kMapProtCpuWrite) == kMapProtCpuWrite) {
    if (!result.empty()) {
      result += " | ";
    }
    result += "CpuWrite";
    prot &= ~kMapProtCpuWrite;
  }
  if ((prot & kMapProtCpuExec) == kMapProtCpuExec) {
    if (!result.empty()) {
      result += " | ";
    }
    result += "CpuExec";
    prot &= ~kMapProtCpuExec;
  }
  if ((prot & kMapProtGpuRead) == kMapProtGpuRead) {
    if (!result.empty()) {
      result += " | ";
    }
    result += "GpuRead";
    prot &= ~kMapProtGpuRead;
  }
  if ((prot & kMapProtGpuWrite) == kMapProtGpuWrite) {
    if (!result.empty()) {
      result += " | ";
    }
    result += "GpuWrite";
    prot &= ~kMapProtGpuWrite;
  }

  if (prot != 0) {
    if (!result.empty()) {
      result += " | ";
    }

    result += std::to_string(prot);
  }

  return result;
}

static constexpr std::uint64_t kPageMask = vm::kPageSize - 1;
static constexpr std::uint64_t kBlockShift = 32;
static constexpr std::uint64_t kBlockSize = static_cast<std::uint64_t>(1)
                                            << kBlockShift;
static constexpr std::uint64_t kBlockMask = kBlockSize - 1;
static constexpr std::uint64_t kPagesInBlock = kBlockSize / vm::kPageSize;
static constexpr std::uint64_t kFirstBlock = 0x00;
static constexpr std::uint64_t kLastBlock = 0xff;
static constexpr std::uint64_t kBlockCount = kLastBlock - kFirstBlock + 1;
static constexpr std::uint64_t kGroupSize = 64;
static constexpr std::uint64_t kGroupMask = kGroupSize - 1;
static constexpr std::uint64_t kGroupsInBlock = kPagesInBlock / kGroupSize;
static constexpr std::uint64_t kMinAddress = orbis::kMinAddress;
static constexpr std::uint64_t kMaxAddress = orbis::kMaxAddress;
static constexpr std::uint64_t kMemorySize = kBlockCount * kBlockSize;

static_assert((kLastBlock + 1) * kBlockSize == orbis::kMaxAddress);

static int gMemoryShm = -1;

struct Group {
  std::uint64_t allocated;
  std::uint64_t shared;
  std::uint64_t readable;
  std::uint64_t writable;
  std::uint64_t executable;
  std::uint64_t gpuReadable;
  std::uint64_t gpuWritable;
};

enum {
  kReadable = vm::kMapProtCpuRead,
  kWritable = vm::kMapProtCpuWrite,
  kExecutable = vm::kMapProtCpuExec,
  kGpuReadable = vm::kMapProtGpuRead,
  kGpuWritable = vm::kMapProtGpuWrite,

  kAllocated = 1 << 3,
  kShared = 1 << 6,
};

inline constexpr std::uint64_t makePagesMask(std::uint64_t page,
                                             std::uint64_t count) {
  if (count == 64) {
    return ~0ull << page;
  }

  return ((1ull << count) - 1ull) << page;
}
struct Block {
  Group groups[kGroupsInBlock];

  void setFlags(std::uint64_t firstPage, std::uint64_t pagesCount,
                std::uint32_t flags, bool noOverwrite) {
    modifyFlags(firstPage, pagesCount, flags, ~static_cast<std::uint32_t>(0),
                noOverwrite);
  }

  void addFlags(std::uint64_t firstPage, std::uint64_t pagesCount,
                std::uint32_t flags) {
    modifyFlags(firstPage, pagesCount, flags, 0);
  }

  void removeFlags(std::uint64_t firstPage, std::uint64_t pagesCount,
                   std::uint32_t flags) {
    modifyFlags(firstPage, pagesCount, 0, flags);
  }

  unsigned getProtection(std::uint64_t page) const {
    std::uint64_t groupIndex = page / kGroupSize;
    auto mask = makePagesMask(page & kGroupMask, 1);
    auto &group = groups[groupIndex];

    if ((group.allocated & mask) == 0) {
      return 0;
    }

    unsigned result = 0;

    result |= (group.readable & mask) == mask ? kReadable : 0;
    result |= (group.writable & mask) == mask ? kReadable | kWritable : 0;

    result |= (group.executable & mask) == mask ? kReadable | kExecutable : 0;

    result |= (group.gpuReadable & mask) == mask ? kGpuReadable : 0;
    result |= (group.gpuWritable & mask) == mask ? kGpuWritable : 0;
    result |= (group.shared & mask) == mask ? kShared : 0;

    return result;
  }

  void modifyFlags(std::uint64_t firstPage, std::uint64_t pagesCount,
                   std::uint32_t addFlags, std::uint32_t removeFlags,
                   bool noOverwrite = false) {
    std::uint64_t groupIndex = firstPage / kGroupSize;

    std::uint64_t addAllocatedFlags =
        (addFlags & kAllocated) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t addSharedFlags =
        (addFlags & kShared) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t addReadableFlags =
        (addFlags & kReadable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t addWritableFlags =
        (addFlags & kWritable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t addExecutableFlags =
        (addFlags & kExecutable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t addGpuReadableFlags =
        (addFlags & kGpuReadable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t addGpuWritableFlags =
        (addFlags & kGpuWritable) ? ~static_cast<std::uint64_t>(0) : 0;

    std::uint64_t removeAllocatedFlags =
        (removeFlags & kAllocated) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t removeSharedFlags =
        (removeFlags & kShared) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t removeReadableFlags =
        (removeFlags & kReadable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t removeWritableFlags =
        (removeFlags & kWritable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t removeExecutableFlags =
        (removeFlags & kExecutable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t removeGpuReadableFlags =
        (removeFlags & kGpuReadable) ? ~static_cast<std::uint64_t>(0) : 0;
    std::uint64_t removeGpuWritableFlags =
        (removeFlags & kGpuWritable) ? ~static_cast<std::uint64_t>(0) : 0;

    if ((firstPage & kGroupMask) != 0) {
      auto count = kGroupSize - (firstPage & kGroupMask);

      if (count > pagesCount) {
        count = pagesCount;
      }

      auto mask = makePagesMask(firstPage & kGroupMask, count);
      pagesCount -= count;

      auto &group = groups[groupIndex++];

      if (noOverwrite) {
        mask &= ~group.allocated;
      }

      group.allocated = (group.allocated & ~(removeAllocatedFlags & mask)) |
                        (addAllocatedFlags & mask);
      group.shared = (group.shared & ~(removeSharedFlags & mask)) |
                     (addSharedFlags & mask);
      group.readable = (group.readable & ~(removeReadableFlags & mask)) |
                       (addReadableFlags & mask);
      group.writable = (group.writable & ~(removeWritableFlags & mask)) |
                       (addWritableFlags & mask);
      group.executable = (group.executable & ~(removeExecutableFlags & mask)) |
                         (addExecutableFlags & mask);
      group.gpuReadable =
          (group.gpuReadable & ~(removeGpuReadableFlags & mask)) |
          (addGpuReadableFlags & mask);
      group.gpuWritable =
          (group.gpuWritable & ~(removeGpuWritableFlags & mask)) |
          (addGpuWritableFlags & mask);
    }

    if (noOverwrite) {
      while (pagesCount >= kGroupSize) {
        pagesCount -= kGroupSize;

        auto &group = groups[groupIndex++];
        auto mask = ~group.allocated;

        group.allocated = (group.allocated & ~(removeAllocatedFlags & mask)) |
                          (addAllocatedFlags & mask);
        group.shared = (group.shared & ~(removeSharedFlags & mask)) |
                       (addSharedFlags & mask);
        group.readable = (group.readable & ~(removeReadableFlags & mask)) |
                         (addReadableFlags & mask);
        group.writable = (group.writable & ~(removeWritableFlags & mask)) |
                         (addWritableFlags & mask);
        group.executable =
            (group.executable & ~(removeExecutableFlags & mask)) |
            (addExecutableFlags & mask);
        group.gpuReadable =
            (group.gpuReadable & ~(removeGpuReadableFlags & mask)) |
            (addGpuReadableFlags & mask);
        group.gpuWritable =
            (group.gpuWritable & ~(removeGpuWritableFlags & mask)) |
            (addGpuWritableFlags & mask);
      }
    } else {
      while (pagesCount >= kGroupSize) {
        pagesCount -= kGroupSize;

        auto &group = groups[groupIndex++];

        group.allocated =
            (group.allocated & ~removeAllocatedFlags) | addAllocatedFlags;
        group.shared = (group.shared & ~removeSharedFlags) | addSharedFlags;
        group.readable =
            (group.readable & ~removeReadableFlags) | addReadableFlags;
        group.writable =
            (group.writable & ~removeWritableFlags) | addWritableFlags;
        group.executable =
            (group.executable & ~removeExecutableFlags) | addExecutableFlags;
        group.gpuReadable =
            (group.gpuReadable & ~removeGpuReadableFlags) | addGpuReadableFlags;
        group.gpuWritable =
            (group.gpuWritable & ~removeGpuWritableFlags) | addGpuWritableFlags;
      }
    }

    if (pagesCount > 0) {
      auto mask = makePagesMask(0, pagesCount);
      auto &group = groups[groupIndex++];

      if (noOverwrite) {
        mask &= ~group.allocated;
      }

      group.allocated = (group.allocated & ~(removeAllocatedFlags & mask)) |
                        (addAllocatedFlags & mask);
      group.shared = (group.shared & ~(removeSharedFlags & mask)) |
                     (addSharedFlags & mask);
      group.readable = (group.readable & ~(removeReadableFlags & mask)) |
                       (addReadableFlags & mask);
      group.writable = (group.writable & ~(removeWritableFlags & mask)) |
                       (addWritableFlags & mask);
      group.executable = (group.executable & ~(removeExecutableFlags & mask)) |
                         (addExecutableFlags & mask);
      group.gpuReadable =
          (group.gpuReadable & ~(removeGpuReadableFlags & mask)) |
          (addGpuReadableFlags & mask);
      group.gpuWritable =
          (group.gpuWritable & ~(removeGpuWritableFlags & mask)) |
          (addGpuWritableFlags & mask);
    }
  }

  bool isFreePages(std::uint64_t page, std::uint64_t count) {
    auto groupIndex = page / kGroupSize;

    std::uint64_t foundCount = 0;

    {
      auto pageInGroup = page % kGroupSize;
      auto allocatedBits = groups[groupIndex].allocated;
      auto freePages = std::countr_zero(allocatedBits >> pageInGroup);

      if (freePages < count && freePages + pageInGroup < kGroupSize) {
        return false;
      }

      foundCount += freePages;
    }

    for (++groupIndex; groupIndex < kGroupsInBlock && foundCount < count;
         ++groupIndex) {
      auto allocatedBits = groups[groupIndex].allocated;
      auto freePages = std::countr_zero(allocatedBits);
      foundCount += freePages;

      if (freePages != kGroupSize) {
        break;
      }
    }

    return foundCount >= count;
  }

  [[nodiscard]] bool isFree() const {
    for (auto &group : groups) {
      if (group.allocated) {
        return false;
      }
    }

    return true;
  }

  std::uint64_t findFreePages(std::uint64_t count, std::uint64_t alignment) {
    std::uint64_t foundCount = 0;
    std::uint64_t foundPage = 0;

    if (alignment < kGroupSize * vm::kPageSize) {
      std::uint64_t groupAlignment = alignment >> vm::kPageShift;

      for (std::uint64_t groupIndex = 0;
           groupIndex < kGroupsInBlock && foundCount < count; ++groupIndex) {
        auto allocatedBits = groups[groupIndex].allocated;

        if (foundCount != 0) {
          // we already found block with free pages at the end
          if (count - foundCount >= kGroupSize) {
            // we need whole group. if it not empty, we need to try next range
            if (allocatedBits != 0) {
              foundCount = 0;
            } else {
              foundCount += kGroupSize;
            }
          } else {
            if (allocatedBits == 0) {
              // whole group is clear, fast path
              foundCount += kGroupSize;
              break;
            } else {
              // add free pages from beginning of the current group
              foundCount += std::countr_zero(allocatedBits);

              if (foundCount >= count) {
                break;
              }

              // not enough free pages, need to try next range
              foundCount = 0;
            }
          }
        }

        if (foundCount == 0) {
          if (~allocatedBits == 0) {
            continue;
          }

          if (count < kGroupSize) {
            // For small allocations try to find free room from beggining of
            // group
            auto tmpAllocatedBits = allocatedBits;
            std::uint64_t processedPages = 0;

            while (processedPages < kGroupSize) {
              auto freeCount = std::countr_zero(tmpAllocatedBits);
              if (freeCount + processedPages > kGroupSize) {
                freeCount = kGroupSize - processedPages;
              }

              processedPages += freeCount;
              if (freeCount >= 64) {
                tmpAllocatedBits = 0;
              } else {
                tmpAllocatedBits >>= freeCount;
              }

              if (freeCount >= count ||
                  (freeCount > 0 && processedPages >= kGroupSize)) {
                foundPage =
                    groupIndex * kGroupSize + processedPages - freeCount;
                foundCount = freeCount;
                break;
              }

              while (auto usedCount = std::countr_one(tmpAllocatedBits)) {
                auto nextProcessedPages =
                    rx::alignUp(processedPages + usedCount, groupAlignment);
                if (nextProcessedPages - processedPages >= 64) {
                  tmpAllocatedBits = 0;
                } else {
                  tmpAllocatedBits >>= nextProcessedPages - processedPages;
                }
                processedPages = nextProcessedPages;
              }
            }
          } else {
            // this is big allocation, count free last pages in block, continue
            // searching on next iterations
            auto freeCount = std::countl_zero(allocatedBits);
            auto alignedPageIndex =
                rx::alignUp(kGroupSize - freeCount, groupAlignment);
            freeCount =
                kGroupSize - alignedPageIndex; // calc aligned free pages

            foundCount = freeCount;
            foundPage = groupIndex * kGroupSize + alignedPageIndex;
          }
        }
      }
    } else {
      std::uint64_t blockAlignment = alignment / (kGroupSize * vm::kPageSize);

      for (std::uint64_t groupIndex = 0;
           groupIndex < kGroupsInBlock && foundCount < count; ++groupIndex) {
        if (foundCount == 0) {
          groupIndex = rx::alignUp(groupIndex, blockAlignment);

          if (groupIndex >= kGroupsInBlock) {
            break;
          }
        }

        auto allocatedBits = groups[groupIndex].allocated;

        if (allocatedBits == 0) {
          if (foundCount == 0) {
            foundPage = groupIndex * kGroupSize;
          }

          foundCount += kGroupSize;
        } else {
          if (foundCount == 0 && count < kGroupSize) {
            auto freeCount = std::countr_zero(allocatedBits);

            if (freeCount >= count) {
              foundPage = groupIndex * kGroupSize;
              foundCount = freeCount;
              break;
            }
          }

          foundCount = 0;
        }
      }
    }

    if (foundCount >= count) {
      assert(((foundPage << vm::kPageShift) & (alignment - 1)) == 0);
      return foundPage;
    }

    return ~static_cast<std::uint64_t>(0);
  }
};

static Block gBlocks[kBlockCount];

struct MapInfo {
  orbis::Ref<IoDevice> device;
  std::uint64_t offset;
  std::uint32_t flags;
  char name[32];

  bool operator==(const MapInfo &) const = default;
};

static rx::MemoryTableWithPayload<MapInfo> gMapInfo;

static void reserve(std::uint64_t startAddress, std::uint64_t endAddress) {
  auto blockIndex = startAddress >> kBlockShift;

  assert(endAddress > startAddress);
  assert(blockIndex == (endAddress >> kBlockShift));

  auto firstPage = (startAddress & kBlockMask) >> vm::kPageShift;
  auto pagesCount =
      (endAddress - startAddress + (vm::kPageSize - 1)) >> vm::kPageShift;

  gBlocks[blockIndex - kFirstBlock].setFlags(firstPage, pagesCount, kAllocated,
                                             false);
}

void vm::fork(std::uint64_t pid) {
  auto shmPath = rx::format("{}/memory-{}", rx::getShmPath(), pid);
  gMemoryShm =
      ::open(shmPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);

  (void)g_mtx.try_lock();
  g_mtx.unlock(); // release mutex

  if (gMemoryShm == -1) {
    rx::println(stderr, "Memory: failed to open {}", shmPath);
    std::abort();
  }

  if (::ftruncate64(gMemoryShm, kMemorySize) < 0) {
    std::println(stderr, "Memory: failed to allocate {}", shmPath);
    std::abort();
  }

  for (auto address = kMinAddress; address < kMaxAddress;
       address += kPageSize) {
    auto prot = gBlocks[(address >> kBlockShift) - kFirstBlock].getProtection(
        (address & kBlockMask) >> vm::kPageShift);

    if (prot & kShared) {
      continue;
    }

    if (prot & kMapProtCpuAll) {
      auto mapping = rx::mem::map(nullptr, kPageSize, PROT_WRITE, MAP_SHARED,
                                  gMemoryShm, address - kMinAddress);
      assert(mapping != MAP_FAILED);

      rx::mem::protect(reinterpret_cast<void *>(address), kPageSize, PROT_READ);
      std::memcpy(mapping, reinterpret_cast<void *>(address), kPageSize);
      rx::mem::unmap(mapping, kPageSize);
      rx::mem::unmap(reinterpret_cast<void *>(address), kPageSize);

      mapping = rx::mem::map(reinterpret_cast<void *>(address), kPageSize,
                             prot & kMapProtCpuAll, MAP_FIXED | MAP_SHARED,
                             gMemoryShm, address - kMinAddress);
      assert(mapping != MAP_FAILED);
    }

    // TODO: copy gpu memory?
  }
}

void vm::reset() {
  std::memset(gBlocks, 0, sizeof(gBlocks));

  rx::mem::unmap(reinterpret_cast<void *>(kMinAddress),
                 kMaxAddress - kMinAddress);
  if (::ftruncate64(gMemoryShm, 0) < 0) {
    std::abort();
  }
  if (::ftruncate64(gMemoryShm, kMemorySize) < 0) {
    std::abort();
  }

  reserve(0, kMinAddress);
  rx::mem::reserve(reinterpret_cast<void *>(kMinAddress),
                   kMaxAddress - kMinAddress);
}

void vm::initialize(std::uint64_t pid) {
  std::println("Memory: initialization");
  auto shmPath = rx::format("{}/memory-{}", rx::getShmPath(), pid);

  gMemoryShm =
      ::open(shmPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);

  if (gMemoryShm == -1) {
    std::println(stderr, "Memory: failed to open {}", shmPath);
    std::abort();
  }

  if (::ftruncate64(gMemoryShm, kMemorySize) < 0) {
    std::println(stderr, "Memory: failed to allocate {}", shmPath);
    std::abort();
  }

  reserve(0, kMinAddress); // unmapped area

  rx::mem::reserve(reinterpret_cast<void *>(kMinAddress),
                   kMaxAddress - kMinAddress);
}

void vm::deinitialize() {
  std::println("Memory: shutdown");
  ::close(gMemoryShm);
  gMemoryShm = -1;

  for (auto &block : gBlocks) {
    block = {};
  }
}

constexpr auto kPhysicalMemorySize = 5568ull * 1024 * 1024;
constexpr auto kFlexibleMemorySize = 448ull * 1024 * 1024;
constexpr auto kMainDirectMemorySize =
    kPhysicalMemorySize - kFlexibleMemorySize;

void *vm::map(void *addr, std::uint64_t len, std::int32_t prot,
              std::int32_t flags, std::int32_t internalFlags, IoDevice *device,
              std::uint64_t offset) {
  std::println(stderr, "vm::map(addr = {}, len = {}, prot = {}, flags = {})",
               addr, len, mapProtToString(prot), mapFlagsToString(flags));

  len = rx::alignUp(len, kPageSize);
  auto pagesCount = (len + (kPageSize - 1)) >> kPageShift;
  auto hitAddress = reinterpret_cast<std::uint64_t>(addr);

  std::uint64_t alignment = (flags & kMapFlagsAlignMask) >> kMapFlagsAlignShift;
  if (alignment == 0) {
    alignment = kPageSize;
  } else {
    alignment = static_cast<std::uint64_t>(1) << alignment;
  }

  if (alignment < kPageSize) {
    std::println(stderr, "Memory error: wrong alignment {}", alignment);
    alignment = kPageSize;
  }

  flags &= ~kMapFlagsAlignMask;

  bool noOverwrite = (flags & (kMapFlagNoOverwrite | kMapFlagFixed)) ==
                     (kMapFlagNoOverwrite | kMapFlagFixed);

  if (hitAddress & (alignment - 1)) {
    if (flags & kMapFlagStack) {
      hitAddress = rx::alignDown(hitAddress - 1, alignment);
      flags |= kMapFlagFixed;
      flags &= ~kMapFlagStack;
    } else {
      hitAddress = rx::alignUp(hitAddress, alignment);
    }
  }

  std::lock_guard lock(g_mtx);

  std::uint64_t address = 0;
  if ((flags & kMapFlagFixed) == kMapFlagFixed) {
    address = hitAddress;

    auto blockIndex = address >> kBlockShift;

    if (blockIndex < kFirstBlock || blockIndex > kLastBlock) {
      std::println(stderr,
                   "Memory error: fixed mapping with wrong address {:x} pages",
                   address);
      return MAP_FAILED;
    }
  } else if (hitAddress != 0) {
    auto blockIndex = hitAddress >> kBlockShift;
    auto page = (hitAddress & kBlockMask) >> kPageShift;

    if (blockIndex < kFirstBlock || blockIndex > kLastBlock) {
      std::println(stderr, "Memory error: wrong hit address {:x} pages",
                   hitAddress);
      hitAddress = 0;
    } else {
      blockIndex -= kFirstBlock;

      if (gBlocks[blockIndex].isFreePages(page, pagesCount)) {
        address = hitAddress;
      }
    }
  }

  static constexpr auto kBadAddress = ~static_cast<std::uint64_t>(0);

  std::size_t mapBlockCount = (len + kBlockSize - 1) / kBlockSize;

  auto isFreeBlockRange = [&](std::uint64_t firstBlock,
                              std::size_t blockCount) {
    for (auto blockIndex = firstBlock; blockIndex < firstBlock + blockCount;
         ++blockIndex) {
      if (!gBlocks[blockIndex].isFree()) {
        return false;
      }
    }

    return true;
  };

  if (address == 0 && hitAddress != 0) {
    auto hitBlockIndex = hitAddress >> kBlockShift;
    if (mapBlockCount > 1) {
      for (auto blockIndex = hitBlockIndex;
           blockIndex <= kLastBlock - mapBlockCount + 1; ++blockIndex) {
        if (isFreeBlockRange(blockIndex, mapBlockCount)) {
          address = blockIndex * kBlockSize;
          break;
        }
      }
    } else {
      for (auto blockIndex = hitBlockIndex; blockIndex <= kLastBlock;
           ++blockIndex) {
        auto pageAddress = gBlocks[blockIndex - kFirstBlock].findFreePages(
            pagesCount, alignment);

        if (pageAddress != kBadAddress) {
          address = (pageAddress << kPageShift) | (blockIndex * kBlockSize);
          break;
        }
      }
    }
  }

  if (address == 0) {
    if (mapBlockCount > 1) {
      for (auto blockIndex = kFirstBlock;
           blockIndex <= kLastBlock - mapBlockCount + 1; ++blockIndex) {
        if (isFreeBlockRange(blockIndex, mapBlockCount)) {
          address = blockIndex * kBlockSize;
          break;
        }
      }
    } else {
      for (auto blockIndex = kFirstBlock; blockIndex <= kLastBlock;
           ++blockIndex) {
        auto pageAddress = gBlocks[blockIndex - kFirstBlock].findFreePages(
            pagesCount, alignment);

        if (pageAddress != kBadAddress) {
          address = (pageAddress << kPageShift) | (blockIndex * kBlockSize);
          break;
        }
      }
    }
  }

  if (address == 0) {
    std::println(stderr,
                 "Memory error: no free memory left for mapping of {} pages",
                 pagesCount);
    return MAP_FAILED;
  }

  if (address & (alignment - 1)) {
    std::println(stderr, "Memory error: failed to map aligned address");
    std::abort();
  }

  if (address >= kMaxAddress || address > kMaxAddress - len) {
    std::println(stderr, "Memory error: out of memory");
    std::abort();
  }

  int realFlags = MAP_FIXED | MAP_SHARED;
  bool isAnon = (flags & kMapFlagAnonymous) == kMapFlagAnonymous;
  bool isShared = (flags & kMapFlagShared) == kMapFlagShared;
  flags &= ~(kMapFlagFixed | kMapFlagAnonymous | kMapFlagShared);

  auto &block = gBlocks[(address >> kBlockShift) - kFirstBlock];
  if (noOverwrite) {
    auto firstPage = (address & kBlockMask) >> kPageShift;
    for (std::size_t i = 0; i < pagesCount; ++i) {
      if (block.getProtection(firstPage + i)) {
        return (void *)-1;
      }
    }
  }

  block.setFlags((address & kBlockMask) >> kPageShift, pagesCount,
                 (prot & (kMapProtCpuAll | kMapProtGpuAll)) | kAllocated |
                     (isShared ? kShared : 0),
                 false);

  /*
    if (flags & kMapFlagStack) {
      realFlags |= MAP_GROWSDOWN | MAP_STACK | MAP_ANONYMOUS | MAP_PRIVATE;
      offset = 0;
      fd = -1;
      flags &= ~kMapFlagStack;
    } else {
      realFlags |= MAP_SHARED;
    }
  */
  if (flags) {
    std::println(stderr, "   unhandled flags 0x{:x}", flags);
  }

  {
    MapInfo info;
    if (auto it = gMapInfo.queryArea(address); it != gMapInfo.end()) {
      info = it.get();
    }
    info.device = device;
    info.flags = flags;
    info.offset = offset;

    gMapInfo.map(address, address + len, info);
  }

  if (auto thr = orbis::g_currentThread) {
    std::lock_guard lock(orbis::g_context.gpuDeviceMtx);
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      gpu.submitMapMemory(thr->tproc->pid, address, len, -1, -1, prot,
                          address - kMinAddress);
    }
  }

  if (internalFlags & kMapInternalReserveOnly) {
    return reinterpret_cast<void *>(address);
  }

  auto result = rx::mem::map(reinterpret_cast<void *>(address), len,
                             prot & kMapProtCpuAll, realFlags, gMemoryShm,
                             address - kMinAddress);

  if (result != MAP_FAILED && isAnon) {
    bool needReprotect = (prot & PROT_WRITE) == 0;
    if (needReprotect) {
      ::mprotect(result, len, PROT_WRITE);
    }
    std::memset(result, 0, len);
    if (needReprotect) {
      ::mprotect(result, len, prot & kMapProtCpuAll);
    }
  }

  return result;
}

bool vm::unmap(void *addr, std::uint64_t size) {
  size = rx::alignUp(size, kPageSize);
  auto pages = (size + (kPageSize - 1)) >> kPageShift;
  auto address = reinterpret_cast<std::uint64_t>(addr);

  if (address < kMinAddress || address >= kMaxAddress || size >= kMaxAddress ||
      address > kMaxAddress - size) {
    std::println(stderr, "Memory error: unmap out of memory");
    return false;
  }

  if ((address & kPageMask) != 0) {
    std::println(stderr, "Memory error: unmap unaligned address");
    return false;
  }

  if ((address >> kBlockShift) != ((address + size - 1) >> kBlockShift)) {
    std::println(
        stderr,
        "Memory error: unmap cross block range. address 0x{:x}, size=0x{:x}",
        address, size);
    __builtin_trap();
  }

  std::lock_guard lock(g_mtx);
  gBlocks[(address >> kBlockShift) - kFirstBlock].removeFlags(
      (address & kBlockMask) >> kPageShift, pages, ~0);
  if (auto thr = orbis::g_currentThread) {
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      gpu.submitUnmapMemory(thr->tproc->pid, address, size);
    }
  } else {
    std::println(stderr, "ignoring unmapping {:x}-{:x}", address,
                 address + size);
  }
  gMapInfo.unmap(address, address + size);
  return rx::mem::unmap(addr, size);
}

bool vm::protect(void *addr, std::uint64_t size, std::int32_t prot) {
  std::println("vm::protect(addr = {}, len = {}, prot = {})", addr, size,
               mapProtToString(prot));

  auto address = reinterpret_cast<std::uint64_t>(addr);
  auto endAddress = address + size;
  address = rx::alignDown(address, kPageSize);
  endAddress = rx::alignUp(endAddress, kPageSize);
  size = endAddress - address;
  auto pages = size >> kPageShift;
  if (address < kMinAddress || address >= kMaxAddress || size >= kMaxAddress ||
      address > kMaxAddress - size) {
    std::println(stderr, "Memory error: protect out of memory");
    return false;
  }

  if ((address & kPageMask) != 0) {
    std::println(stderr, "Memory error: protect unaligned address");
    return false;
  }

  if ((address >> kBlockShift) != ((address + size - 1) >> kBlockShift)) {
    std::println(stderr, "Memory error: protect cross block range");
    std::abort();
  }

  std::lock_guard lock(g_mtx);

  gBlocks[(address >> kBlockShift) - kFirstBlock].setFlags(
      (address & kBlockMask) >> kPageShift, pages,
      kAllocated | (prot & (kMapProtCpuAll | kMapProtGpuAll)), false);

  if (auto thr = orbis::g_currentThread) {
    std::println("memory prot: {:x}", prot);
    if (auto gpu = amdgpu::DeviceCtl{orbis::g_context.gpuDevice}) {
      gpu.submitProtectMemory(thr->tproc->pid, address, size, prot);
    }
  } else if (prot >> 4) {
    std::println(stderr, "ignoring mapping {:x}-{:x}", address, address + size);
  }
  return ::mprotect(std::bit_cast<void *>(address), size,
                    prot & kMapProtCpuAll) == 0;
}

static std::int32_t getPageProtectionImpl(std::uint64_t address) {
  return gBlocks[(address >> kBlockShift) - kFirstBlock].getProtection(
             (address & kBlockMask) >> vm::kPageShift) &
         ~kShared;
}

bool vm::queryProtection(const void *addr, std::uint64_t *startAddress,
                         std::uint64_t *endAddress, std::int32_t *prot) {
  auto address = reinterpret_cast<std::uintptr_t>(addr);
  if (address < kMinAddress || address >= kMaxAddress) {
    return false;
  }

  std::uint64_t start = address & ~kPageMask;
  std::uint64_t end = start + kPageSize;

  std::lock_guard lock(g_mtx);

  auto resultProt = getPageProtectionImpl(address);

  while (true) {
    auto testAddr = start - kPageSize;
    if (testAddr < kMinAddress) {
      break;
    }

    auto testProt = getPageProtectionImpl(testAddr);

    if (resultProt != testProt) {
      break;
    }

    start = testAddr;
  }

  while (true) {
    auto testAddr = end;
    if (testAddr >= kMaxAddress) {
      break;
    }

    auto testProt = getPageProtectionImpl(testAddr);

    if (resultProt != testProt) {
      break;
    }

    end = testAddr + kPageSize;
  }

  *startAddress = start;
  *endAddress = end;
  *prot = resultProt;

  return true;
}

unsigned vm::getPageProtection(std::uint64_t address) {
  if (address < kMinAddress || address >= kMaxAddress) {
    std::println(stderr, "Memory error: getPageProtection out of memory");
    return 0;
  }

  std::lock_guard lock(g_mtx);

  return getPageProtectionImpl(address);
}

bool vm::virtualQuery(const void *addr, std::int32_t flags,
                      VirtualQueryInfo *info) {
  std::lock_guard lock(g_mtx);

  auto address = reinterpret_cast<std::uint64_t>(addr);
  if (address < kMinAddress || address >= kMaxAddress) {
    return false;
  }

  auto it = gMapInfo.lowerBound(address);

  if (it == gMapInfo.end()) {
    return false;
  }

  if ((flags & 1) == 0) {
    if (it.endAddress() <= address) {
      return false;
    }
  } else {
    if (it.beginAddress() > address || it.endAddress() <= address) {
      return false;
    }
  }

  std::int32_t memoryType = 0;
  std::uint32_t blockFlags = 0;
  if (it->device != nullptr) {
    if (auto dmem = dynamic_cast<DmemDevice *>(it->device.get())) {
      auto dmemIt = dmem->allocations.queryArea(it->offset);
      if (dmemIt == dmem->allocations.end()) {
        return false;
      }
      memoryType = dmemIt->memoryType;
      blockFlags = kBlockFlagDirectMemory;
      rx::print(stderr, "virtual query {}", addr);
      rx::println(stderr, "memory type: {}", memoryType);
    }
    // TODO
  }

  std::int32_t prot = getPageProtectionImpl(it.beginAddress());

  *info = {
      .start = it.beginAddress(),
      .end = it.endAddress(),
      .protection = prot,
      .memoryType = memoryType,
      .flags = blockFlags,
  };

  ORBIS_LOG_ERROR("virtualQuery", addr, flags, info->start, info->end,
                  info->protection, info->memoryType, info->flags);

  std::memcpy(info->name, it->name, sizeof(info->name));
  return true;
}

void vm::setName(std::uint64_t start, std::uint64_t size, const char *name) {
  std::lock_guard lock(g_mtx);

  MapInfo info;
  if (auto it = gMapInfo.queryArea(start); it != gMapInfo.end()) {
    info = it.get();
  }

  std::strncpy(info.name, name, sizeof(info.name));

  gMapInfo.map(start, size, info);
}
