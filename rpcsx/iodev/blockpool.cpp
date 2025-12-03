#include "orbis/blockpool.hpp"
#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/dmem.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/vmem.hpp"
#include "rx/AddressRange.hpp"

enum {
  BLOCKPOOL_IOCTL_EXPAND = 0xc020a801,
  BLOCKPOOL_IOCTL_GET_BLOCK_STATS = 0x4010a802,
};

struct BlockPoolAllocation {
  bool allocated = false;

  [[nodiscard]] bool isAllocated() const { return allocated; }
  [[nodiscard]] bool isRelated(const BlockPoolAllocation &, rx::AddressRange,
                               rx::AddressRange) const {
    return true;
  }

  [[nodiscard]] BlockPoolAllocation merge(const BlockPoolAllocation &other,
                                          rx::AddressRange,
                                          rx::AddressRange) const {
    return other;
  }

  bool operator==(const BlockPoolAllocation &) const = default;
};

using DirectMemoryResource =
    kernel::AllocableResource<BlockPoolAllocation, orbis::kallocator>;

struct BlockPoolFile : public orbis::File {};
struct BlockPoolDevice
    : orbis::IoDeviceWithIoctl<orbis::ioctl::group(BLOCKPOOL_IOCTL_EXPAND)> {
  rx::shared_mutex mtx;
  rx::MemoryAreaTable<> pool;
  orbis::sint availBlocks{};
  orbis::sint commitBlocks{};

  BlockPoolDevice();

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
  orbis::ErrorCode map(rx::AddressRange range, std::int64_t offset,
                       rx::EnumBitSet<orbis::vmem::Protection> protection,
                       orbis::File *file, orbis::Process *process) override;

  [[nodiscard]] std::string toString() const override { return "blockpool"; }
};

#pragma pack(push, 1)

struct BlockPoolIoctlExpand {
  orbis::uint64_t len;
  orbis::uint64_t searchStart;
  orbis::uint64_t searchEnd;
  orbis::uint32_t flags;
  orbis::uint32_t _padding;
};
#pragma pack(pop)

static orbis::ErrorCode blockpool_ioctl_expand(orbis::Thread *thread,
                                               BlockPoolDevice *device,
                                               BlockPoolIoctlExpand &args) {
  ORBIS_LOG_TODO(__FUNCTION__, args.len, args.searchStart, args.searchEnd,
                 args.flags);

  if (args.len % orbis::dmem::kPageSize || args.len == 0) {
    return orbis::ErrorCode::INVAL;
  }

  auto alignment = args.flags == 0 ? 0 : 1ull << ((args.flags >> 24) & 0x1f);

  auto [dmemOffset, dmemErrc] = orbis::dmem::allocate(
      0, rx::AddressRange::fromBeginEnd(args.searchStart, args.searchEnd),
      args.len, orbis::MemoryType::WbOnion, alignment, true);

  if (dmemErrc != orbis::ErrorCode{}) {
    return dmemErrc;
  }

  auto [pmemOffset, pmemErrc] = orbis::dmem::getPmemOffset(0, dmemOffset);

  if (pmemErrc != orbis::ErrorCode{}) {
    return pmemErrc;
  }

  args.searchStart = dmemOffset;
  return orbis::blockpool::expand(
      rx::AddressRange::fromBeginSize(pmemOffset, args.len));
}

static std::pair<orbis::ErrorCode, orbis::blockpool::BlockStats>
blockpool_ioctl_get_block_stats(orbis::Thread *, BlockPoolDevice *) {
  return {{}, orbis::blockpool::stats()};
}

static const orbis::FileOps ops = {};

BlockPoolDevice::BlockPoolDevice() {
  blockFlags = orbis::vmem::BlockFlags::PooledMemory;

  addIoctl<BLOCKPOOL_IOCTL_EXPAND>(blockpool_ioctl_expand);
  addIoctl<BLOCKPOOL_IOCTL_GET_BLOCK_STATS>(blockpool_ioctl_get_block_stats);
}

orbis::ErrorCode BlockPoolDevice::open(rx::Ref<orbis::File> *file,
                                       const char *path, std::uint32_t flags,
                                       std::uint32_t mode,
                                       orbis::Thread *thread) {
  auto newFile = orbis::knew<BlockPoolFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::ErrorCode
BlockPoolDevice::map(rx::AddressRange range, std::int64_t offset,
                     rx::EnumBitSet<orbis::vmem::Protection> protection,
                     orbis::File *file, orbis::Process *process) {
  if (protection || offset != 0) {
    return orbis::ErrorCode::INVAL;
  }

  return {};
}

orbis::IoDevice *createBlockPoolDevice() {
  return orbis::knew<BlockPoolDevice>();
}
