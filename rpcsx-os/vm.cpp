#include "vm.hpp"
#include "align.hpp"
#include "bridge.hpp"
#include "io-device.hpp"
#include "iodev/dmem.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/Rc.hpp"
#include "rx/mem.hpp"
#include <bit>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>

#include <rx/MemoryTable.hpp>

static std::mutex g_mtx;

std::string rx::vm::mapFlagsToString(std::int32_t flags) {
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

std::string rx::vm::mapProtToString(std::int32_t prot) {
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

static constexpr std::uint64_t kPageMask = rx::vm::kPageSize - 1;
static constexpr std::uint64_t kBlockShift = 32;
static constexpr std::uint64_t kBlockSize = static_cast<std::uint64_t>(1)
                                            << kBlockShift;
static constexpr std::uint64_t kBlockMask = kBlockSize - 1;
static constexpr std::uint64_t kPagesInBlock = kBlockSize / rx::vm::kPageSize;
static constexpr std::uint64_t kFirstBlock = 0x00;
static constexpr std::uint64_t kLastBlock = 0xff;
static constexpr std::uint64_t kBlockCount = kLastBlock - kFirstBlock + 1;
static constexpr std::uint64_t kGroupSize = 64;
static constexpr std::uint64_t kGroupMask = kGroupSize - 1;
static constexpr std::uint64_t kGroupsInBlock = kPagesInBlock / kGroupSize;
static constexpr std::uint64_t kMinAddress =
    kFirstBlock * kBlockSize + rx::vm::kPageSize * 0x10;
static constexpr std::uint64_t kMaxAddress = (kLastBlock + 1) * kBlockSize - 1;
static constexpr std::uint64_t kMemorySize = kBlockCount * kBlockSize;

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
  kReadable = rx::vm::kMapProtCpuRead,
  kWritable = rx::vm::kMapProtCpuWrite,
  kExecutable = rx::vm::kMapProtCpuExec,
  kGpuReadable = rx::vm::kMapProtGpuRead,
  kGpuWritable = rx::vm::kMapProtGpuWrite,

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

  std::uint64_t findFreePages(std::uint64_t count, std::uint64_t alignment) {
    std::uint64_t foundCount = 0;
    std::uint64_t foundPage = 0;

    if (alignment < kGroupSize * rx::vm::kPageSize) {
      std::uint64_t groupAlignment = alignment >> rx::vm::kPageShift;

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
                    utils::alignUp(processedPages + usedCount, groupAlignment);
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
                utils::alignUp(kGroupSize - freeCount, groupAlignment);
            freeCount =
                kGroupSize - alignedPageIndex; // calc aligned free pages

            foundCount = freeCount;
            foundPage = groupIndex * kGroupSize + alignedPageIndex;
          }
        }
      }
    } else {
      std::uint64_t blockAlignment =
          alignment / (kGroupSize * rx::vm::kPageSize);

      for (std::uint64_t groupIndex = 0;
           groupIndex < kGroupsInBlock && foundCount < count; ++groupIndex) {
        if (foundCount == 0) {
          groupIndex = utils::alignUp(groupIndex, blockAlignment);

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
      assert(((foundPage << rx::vm::kPageShift) & (alignment - 1)) == 0);
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

  auto firstPage = (startAddress & kBlockMask) >> rx::vm::kPageShift;
  auto pagesCount = (endAddress - startAddress + (rx::vm::kPageSize - 1)) >>
                    rx::vm::kPageShift;

  gBlocks[blockIndex - kFirstBlock].setFlags(firstPage, pagesCount, kAllocated,
                                             false);
}

void rx::vm::fork(std::uint64_t pid) {
  gMemoryShm = ::shm_open(("/rpcsx-os-memory-" + std::to_string(pid)).c_str(),
                          O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  (void)g_mtx.try_lock();
  g_mtx.unlock(); // release mutex

  if (gMemoryShm == -1) {
    std::fprintf(stderr, "Memory: failed to open /rpcsx-os-memory\n");
    std::abort();
  }

  if (::ftruncate64(gMemoryShm, kMemorySize) < 0) {
    std::fprintf(stderr, "Memory: failed to allocate /rpcsx-os-memory\n");
    std::abort();
  }

  for (auto address = kMinAddress; address < kMaxAddress;
       address += kPageSize) {
    auto prot = gBlocks[(address >> kBlockShift) - kFirstBlock].getProtection(
        (address & kBlockMask) >> rx::vm::kPageShift);

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

void rx::vm::reset() {
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

void rx::vm::initialize(std::uint64_t pid) {
  std::printf("Memory: initialization\n");

  gMemoryShm = ::shm_open(("/rpcsx-os-memory-" + std::to_string(pid)).c_str(),
                          O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  if (gMemoryShm == -1) {
    std::fprintf(stderr, "Memory: failed to open /rpcsx-os-memory\n");
    std::abort();
  }

  if (::ftruncate64(gMemoryShm, kMemorySize) < 0) {
    std::fprintf(stderr, "Memory: failed to allocate /rpcsx-os-memory\n");
    std::abort();
  }

  reserve(0, kMinAddress); // unmapped area

  rx::mem::reserve(reinterpret_cast<void *>(kMinAddress),
                   kMaxAddress - kMinAddress);

  // orbis::bridge.setUpSharedMemory(kMinAddress, kMemorySize, "/orbis-memory");
}

void rx::vm::deinitialize() {
  std::printf("Memory: shutdown\n");
  ::close(gMemoryShm);
  gMemoryShm = -1;
  ::shm_unlink("/orbis-memory");

  for (auto &block : gBlocks) {
    block = {};
  }
}

constexpr auto kPhysicalMemorySize = 5568ull * 1024 * 1024;
constexpr auto kFlexibleMemorySize = 448ull * 1024 * 1024;
constexpr auto kMainDirectMemorySize =
    kPhysicalMemorySize - kFlexibleMemorySize;

void *rx::vm::map(void *addr, std::uint64_t len, std::int32_t prot,
                  std::int32_t flags, std::int32_t internalFlags,
                  IoDevice *device, std::uint64_t offset) {
  std::fprintf(stderr,
               "rx::vm::map(addr = %p, len = %" PRIu64
               ", prot = %s, flags = %s)\n",
               addr, len, mapProtToString(prot).c_str(),
               mapFlagsToString(flags).c_str());

  len = utils::alignUp(len, kPageSize);
  auto pagesCount = (len + (kPageSize - 1)) >> kPageShift;
  auto hitAddress = reinterpret_cast<std::uint64_t>(addr);

  std::uint64_t alignment = (flags & kMapFlagsAlignMask) >> kMapFlagsAlignShift;
  if (alignment == 0) {
    alignment = kPageSize;
  } else {
    alignment = static_cast<std::uint64_t>(1) << alignment;
  }

  if (alignment < kPageSize) {
    std::fprintf(stderr, "Memory error: wrong alignment %" PRId64 "\n",
                 alignment);
    alignment = kPageSize;
  }

  if (len > kBlockSize) {
    std::fprintf(stderr, "Memory error: too big allocation %" PRId64 " pages\n",
                 pagesCount);
    return MAP_FAILED;
  }

  flags &= ~kMapFlagsAlignMask;

  bool noOverwrite = (flags & (kMapFlagNoOverwrite | kMapFlagFixed)) ==
                     (kMapFlagNoOverwrite | kMapFlagFixed);

  if (hitAddress & (alignment - 1)) {
    if (flags & kMapFlagStack) {
      hitAddress = utils::alignDown(hitAddress - 1, alignment);
      flags |= kMapFlagFixed;
      flags &= ~kMapFlagStack;
    } else {
      hitAddress = utils::alignUp(hitAddress, alignment);
    }
  }

  std::lock_guard lock(g_mtx);

  std::uint64_t address = 0;
  if ((flags & kMapFlagFixed) == kMapFlagFixed) {
    address = hitAddress;

    auto blockIndex = address >> kBlockShift;

    if (blockIndex < kFirstBlock || blockIndex > kLastBlock) {
      std::fprintf(stderr,
                   "Memory error: fixed mapping with wrong address %" PRIx64
                   " pages\n",
                   address);
      return MAP_FAILED;
    }
  } else if (hitAddress != 0) {
    auto blockIndex = hitAddress >> kBlockShift;
    auto page = (hitAddress & kBlockMask) >> kPageShift;

    if (blockIndex < kFirstBlock || blockIndex > kLastBlock) {
      std::fprintf(stderr,
                   "Memory error: wrong hit address %" PRIx64 " pages\n",
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

  if (address == 0 && hitAddress != 0) {
    auto hitBlockIndex = hitAddress >> kBlockShift;
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

  if (address == 0) {
    for (auto blockIndex = kFirstBlock; blockIndex <= 2; ++blockIndex) {
      // std::size_t blockIndex = 0; // system managed block

      auto pageAddress = gBlocks[blockIndex - kFirstBlock].findFreePages(
          pagesCount, alignment);

      if (pageAddress != kBadAddress) {
        address = (pageAddress << kPageShift) | (blockIndex * kBlockSize);
        break;
      }
    }
  }

  if (address == 0) {
    std::fprintf(stderr,
                 "Memory error: no free memory left for mapping of %" PRId64
                 " pages\n",
                 pagesCount);
    return MAP_FAILED;
  }

  if (address & (alignment - 1)) {
    std::fprintf(stderr, "Memory error: failed to map aligned address\n");
    std::abort();
  }

  if (address >= kMaxAddress || address > kMaxAddress - len) {
    std::fprintf(stderr, "Memory error: out of memory\n");
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
    std::fprintf(stderr, "   unhandled flags 0x%" PRIx32 "\n", flags);
  }

  {
    MapInfo info;
    if (auto it = gMapInfo.queryArea(address); it != gMapInfo.end()) {
      info = (*it).payload;
    }
    info.device = device;
    info.flags = flags;
    info.offset = offset;

    gMapInfo.map(address, address + len, info);
  }

  // if (device == nullptr) {
    if (auto thr = orbis::g_currentThread) {
      rx::bridge.sendMapMemory(thr->tproc->pid, -1, -1, address, len, prot,
                               address - kMinAddress);
    } else {
      std::fprintf(stderr, "ignoring mapping %lx-%lx\n", address,
                   address + len);
    }
  // }

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

bool rx::vm::unmap(void *addr, std::uint64_t size) {
  size = utils::alignUp(size, kPageSize);
  auto pages = (size + (kPageSize - 1)) >> kPageShift;
  auto address = reinterpret_cast<std::uint64_t>(addr);

  if (address < kMinAddress || address >= kMaxAddress || size > kMaxAddress ||
      address > kMaxAddress - size) {
    std::fprintf(stderr, "Memory error: unmap out of memory\n");
    return false;
  }

  if ((address & kPageMask) != 0) {
    std::fprintf(stderr, "Memory error: unmap unaligned address\n");
    return false;
  }

  if ((address >> kBlockShift) != ((address + size - 1) >> kBlockShift)) {
    std::fprintf(
        stderr,
        "Memory error: unmap cross block range. address 0x%lx, size=0x%lx\n",
        address, size);
    __builtin_trap();
  }

  std::lock_guard lock(g_mtx);
  gBlocks[(address >> kBlockShift) - kFirstBlock].removeFlags(
      (address & kBlockMask) >> kPageShift, pages, ~0);
  if (auto thr = orbis::g_currentThread) {
    rx::bridge.sendMemoryProtect(
        thr->tproc->pid, reinterpret_cast<std::uint64_t>(addr), size, 0);
  } else {
    std::fprintf(stderr, "ignoring mapping %lx-%lx\n", address, address + size);
  }
  return rx::mem::unmap(addr, size);
}

bool rx::vm::protect(void *addr, std::uint64_t size, std::int32_t prot) {
  std::printf("rx::vm::protect(addr = %p, len = %" PRIu64 ", prot = %s)\n",
              addr, size, mapProtToString(prot).c_str());

  size = utils::alignUp(size, kPageSize);
  auto pages = (size + (kPageSize - 1)) >> kPageShift;
  auto address = reinterpret_cast<std::uint64_t>(addr);
  if (address < kMinAddress || address >= kMaxAddress || size > kMaxAddress ||
      address > kMaxAddress - size) {
    std::fprintf(stderr, "Memory error: protect out of memory\n");
    return false;
  }

  if ((address & kPageMask) != 0) {
    std::fprintf(stderr, "Memory error: protect unaligned address\n");
    return false;
  }

  if ((address >> kBlockShift) != ((address + size - 1) >> kBlockShift)) {
    std::fprintf(stderr, "Memory error: protect cross block range\n");
    std::abort();
  }

  std::lock_guard lock(g_mtx);

  gBlocks[(address >> kBlockShift) - kFirstBlock].setFlags(
      (address & kBlockMask) >> kPageShift, pages,
      kAllocated | (prot & (kMapProtCpuAll | kMapProtGpuAll)), false);

  if (auto thr = orbis::g_currentThread) {
    std::printf("memory prot: %x\n", prot);
    rx::bridge.sendMemoryProtect(
        thr->tproc->pid, reinterpret_cast<std::uint64_t>(addr), size, prot);
  } else {
    std::fprintf(stderr, "ignoring mapping %lx-%lx\n", address, address + size);
  }
  return ::mprotect(addr, size, prot & kMapProtCpuAll) == 0;
}

static std::int32_t getPageProtectionImpl(std::uint64_t address) {
  return gBlocks[(address >> kBlockShift) - kFirstBlock].getProtection(
             (address & kBlockMask) >> rx::vm::kPageShift) &
         ~kShared;
}

bool rx::vm::queryProtection(const void *addr, std::uint64_t *startAddress,
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

unsigned rx::vm::getPageProtection(std::uint64_t address) {
  if (address < kMinAddress || address >= kMaxAddress) {
    std::fprintf(stderr, "Memory error: getPageProtection out of memory\n");
    return 0;
  }

  std::lock_guard lock(g_mtx);

  return getPageProtectionImpl(address);
}

bool rx::vm::virtualQuery(const void *addr, std::int32_t flags,
                          VirtualQueryInfo *info) {
  std::lock_guard lock(g_mtx);

  auto address = reinterpret_cast<std::uint64_t>(addr);
  auto it = gMapInfo.lowerBound(address);

  if (it == gMapInfo.end()) {
    return false;
  }

  auto queryInfo = *it;

  if ((flags & 1) == 0) {
    if (queryInfo.endAddress <= address) {
      return false;
    }
  } else {
    if (queryInfo.beginAddress > address || queryInfo.endAddress <= address) {
      return false;
    }
  }

  std::int32_t memoryType = 0;
  std::uint32_t blockFlags = 0;
  if (queryInfo.payload.device != nullptr) {
    if (auto dmem =
            dynamic_cast<DmemDevice *>(queryInfo.payload.device.get())) {
      auto dmemIt = dmem->allocations.queryArea(queryInfo.payload.offset);
      if (dmemIt == dmem->allocations.end()) {
        return false;
      }
      auto alloc = *dmemIt;
      memoryType = alloc.payload.memoryType;
      blockFlags = kBlockFlagDirectMemory;
      std::fprintf(stderr, "virtual query %p", addr);
      std::fprintf(stderr, "memory type: %u\n", memoryType);
    }
    // TODO
  }

  std::int32_t prot = getPageProtectionImpl(queryInfo.beginAddress);

  *info = {
      .start = queryInfo.beginAddress,
      .end = queryInfo.endAddress,
      .protection = prot,
      .memoryType = memoryType,
      .flags = blockFlags,
  };

  ORBIS_LOG_ERROR("virtualQuery", addr, flags, info->start, info->end,
                  info->protection, info->memoryType, info->flags);

  std::memcpy(info->name, queryInfo.payload.name, sizeof(info->name));
  return true;
}

void rx::vm::setName(std::uint64_t start, std::uint64_t size,
                     const char *name) {
  std::lock_guard lock(g_mtx);

  MapInfo info;
  if (auto it = gMapInfo.queryArea(start); it != gMapInfo.end()) {
    info = (*it).payload;
  }

  std::strncpy(info.name, name, sizeof(info.name));

  gMapInfo.map(start, size, info);
}

void rx::vm::printHostStats() {
  FILE *maps = fopen("/proc/self/maps", "r");

  if (!maps) {
    return;
  }

  char *line = nullptr;
  std::size_t size = 0;
  while (getline(&line, &size, maps) > 0) {
    std::printf("%s", line);
  }

  free(line);
  fclose(maps);
}
