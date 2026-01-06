#include "orbis/dmem.hpp"
#include "gpu/DeviceCtl.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/error.hpp"
#include "orbis/file.hpp"
#include "orbis/pmem.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/vmem.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include "rx/format.hpp"
#include <string>

enum {
  DMEM_IOCTL_ALLOCATE = 0xc0288001,
  DMEM_IOCTL_RELEASE = 0x80108002,
  DMEM_IOCTL_SET_TYPE = 0x80188003,
  DMEM_IOCTL_GET_TYPE = 0xc0208004,
  DMEM_IOCTL_GET_TOTAL_SIZE = 0x4008800a,
  DMEM_IOCTL_CLEAR = 0x2000800b,
  DMEM_IOCTL_TRANSFER_BUDGET = 0xc018800d,
  DMEM_IOCTL_CONTROL_RELEASE = 0xc018800e,
  DMEM_IOCTL_SET_PID_AND_PROTECT = 0xc018800f,
  DMEM_IOCTL_ALLOCATE_FOR_MINI_APP = 0xc0288010,
  DMEM_IOCTL_ALLOCATE_MAIN = 0xc0288011,
  DMEM_IOCTL_QUERY = 0x80288012,
  DMEM_IOCTL_CHECKED_RELEASE = 0x80108015,
  DMEM_IOCTL_GET_AVAIL_SIZE = 0xc0208016,
  DMEM_IOCTL_RESERVE = 0xc010801a,
};
struct DmemDevice
    : orbis::IoDeviceWithIoctl<orbis::ioctl::group(DMEM_IOCTL_ALLOCATE)> {
  int index;
  DmemDevice(int index);

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  orbis::ErrorCode map(rx::AddressRange range, std::int64_t offset,
                       rx::EnumBitSet<orbis::vmem::Protection> protection,
                       orbis::File *file, orbis::Process *process) override;

  std::pair<rx::AddressRange, orbis::MemoryType>
  getPmemRange(std::uint64_t offset, orbis::File *) override {
    return orbis::dmem::getPmemRange(index, offset);
  }

  [[nodiscard]] std::string toString() const override {
    return "dmem" + std::to_string(index);
  }
};

struct DmemFile : public orbis::File {};

#pragma pack(push, 1)
struct DmemIoctlAllocate {
  orbis::uintptr_t searchStart;
  orbis::uintptr_t searchEnd;
  orbis::size_t len;
  orbis::size_t alignment;
  orbis::MemoryType memoryType;
  orbis::uint32_t padding;
};

struct DmemIoctlRelease {
  orbis::uintptr_t address;
  orbis::size_t size;
};
struct DmemIoctlSetType {
  orbis::uintptr_t start;
  orbis::uintptr_t end;
  orbis::MemoryType memoryType;
  orbis::uint32_t padding;
};
struct DmemIoctlGetType {
  orbis::uintptr_t start;
  orbis::uintptr_t regionStart;
  orbis::uintptr_t regionEnd;
  orbis::MemoryType memoryType;
  orbis::uint32_t padding;
};
struct DmemIoctlTransferBudget {
  orbis::uint64_t unk0;
  orbis::uint64_t unk1;
  orbis::uint64_t unk2;
};
struct DmemIoctlControlRelease {
  orbis::uint64_t unk0;
  orbis::uint64_t unk1;
  orbis::uint64_t unk2;
};
struct DmemIoctlSetPidAndProtect {
  orbis::uintptr_t offset;
  orbis::size_t size;
  orbis::pid_t pid; // 0 if all
  rx::EnumBitSet<orbis::vmem::Protection> prot;
};

struct DirectMemoryQueryInfo {
  orbis::uintptr_t start;
  orbis::uintptr_t end;
  orbis::MemoryType memoryType;
  orbis::uint32_t padding;
};
struct DmemIoctlQuery {
  orbis::uint32_t devIndex;
  orbis::uint32_t flags;
  orbis::uint32_t unk;
  orbis::uint32_t _padding;
  orbis::uint64_t offset;
  orbis::ptr<DirectMemoryQueryInfo> info;
  orbis::uint64_t infoSize;
};
struct DmemIoctlGetAvailSize {
  orbis::uintptr_t searchStart;
  orbis::uintptr_t searchEnd;
  orbis::size_t alignment;
  orbis::size_t size;
};

struct DmemIoctlReserve {
  orbis::size_t size;
  orbis::uint32_t flags;
  orbis::uint32_t padding;
};
#pragma pack(pop)

static orbis::ErrorCode dmem_ioctl_allocate(orbis::Thread *thread,
                                            DmemDevice *device,
                                            DmemIoctlAllocate &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.searchStart, args.searchEnd,
                    args.alignment, args.len, (int)args.memoryType);
  auto [offset, errc] = orbis::dmem::allocate(
      device->index,
      rx::AddressRange::fromBeginEnd(args.searchStart, args.searchEnd),
      args.len, args.memoryType);

  if (errc != orbis::ErrorCode{}) {
    return errc;
  }

  args.searchStart = offset;
  return {};
}

static orbis::ErrorCode dmem_ioctl_release(orbis::Thread *thread,
                                           DmemDevice *device,
                                           const DmemIoctlRelease &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.address, args.size);

  return orbis::dmem::release(
      device->index, rx::AddressRange::fromBeginSize(args.address, args.size));
}

static orbis::ErrorCode dmem_ioctl_set_type(orbis::Thread *thread,
                                            DmemDevice *device,
                                            const DmemIoctlSetType &args) {
  // removed ioctl
  return orbis::ErrorCode::INVAL;
}

static orbis::ErrorCode dmem_ioctl_get_type(orbis::Thread *thread,
                                            DmemDevice *device,
                                            DmemIoctlGetType &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.start);

  auto result = orbis::dmem::query(device->index, args.start);

  if (!result) {
    return orbis::ErrorCode::NOENT;
  }

  args.regionStart = result->range.beginAddress();
  args.regionEnd = result->range.endAddress();
  args.memoryType = result->memoryType;
  return {};
}

static std::pair<orbis::ErrorCode, orbis::uint64_t>
dmem_ioctl_get_total_size(orbis::Thread *thread, DmemDevice *device) {
  auto result = orbis::dmem::getSize(device->index);
  ORBIS_LOG_WARNING(__FUNCTION__, result);

  auto limit = thread->tproc->getBudget()->get(orbis::BudgetResource::Dmem);
  return {{}, orbis::uint64_t(std::min(result, limit.total))};
}

static orbis::ErrorCode dmem_ioctl_clear(orbis::Thread *thread,
                                         DmemDevice *device) {
  ORBIS_LOG_WARNING(__FUNCTION__);
  auto result = orbis::dmem::clear(device->index);
  if (result == orbis::ErrorCode{}) {
    thread->tproc->getBudget()->release(orbis::BudgetResource::Dmem, -1);
  }
  return result;
}

static orbis::ErrorCode
dmem_ioctl_transfer_budget(orbis::Thread *thread, DmemDevice *device,
                           DmemIoctlTransferBudget &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.unk0, args.unk1, args.unk2);
  return {};
}

static orbis::ErrorCode
dmem_ioctl_control_release(orbis::Thread *thread, DmemDevice *device,
                           DmemIoctlControlRelease &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.unk0, args.unk1, args.unk2);
  return {};
}

static orbis::ErrorCode
dmem_ioctl_set_pid_and_protect(orbis::Thread *thread, DmemDevice *device,
                               DmemIoctlSetPidAndProtect &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.pid, args.offset, args.size,
                    args.prot.toUnderlying());

  orbis::Process *process = nullptr;

  if (args.pid != 0) {
    process = args.pid == -1 || args.pid == thread->tproc->pid
                  ? thread->tproc
                  : orbis::findProcessById(args.pid);
    if (process == nullptr) {
      return orbis::ErrorCode::SRCH;
    }
  }

  return orbis::dmem::protect(
      process, 0, rx::AddressRange::fromBeginSize(args.offset, args.size),
      args.prot);
}

static orbis::ErrorCode
dmem_ioctl_allocate_for_mini_app(orbis::Thread *thread, DmemDevice *device,
                                 DmemIoctlAllocate &args) {
  // FIXME: implement
  ORBIS_LOG_WARNING(__FUNCTION__, args.searchStart, args.searchEnd,
                    args.alignment, args.len, (int)args.memoryType);

  return dmem_ioctl_allocate(thread, device, args);
}

static orbis::ErrorCode dmem_ioctl_allocate_main(orbis::Thread *thread,
                                                 DmemDevice *device,
                                                 DmemIoctlAllocate &args) {
  // FIXME: implement
  ORBIS_LOG_WARNING(__FUNCTION__, args.searchStart, args.searchEnd,
                    args.alignment, args.len, (int)args.memoryType);

  return dmem_ioctl_allocate(thread, device, args);
}

static orbis::ErrorCode dmem_ioctl_query(orbis::Thread *thread,
                                         DmemDevice *device,
                                         const DmemIoctlQuery &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, device->index, args.devIndex, args.unk,
                    args.flags, args.offset, args.info, args.infoSize);

  if (args.devIndex != device->index) {
    ORBIS_LOG_WARNING(__FUNCTION__, "device mismatch", device->index,
                      args.devIndex, args.unk, args.flags, args.offset,
                      args.info, args.infoSize);
  }

  if (args.infoSize != sizeof(DirectMemoryQueryInfo) || args.devIndex >= 3) {
    return orbis::ErrorCode::INVAL;
  }

  rx::EnumBitSet<orbis::dmem::QueryFlags> queryFlags = {};

  if (args.flags & 1) {
    queryFlags |= orbis::dmem::QueryFlags::LowerBound;
  }

  auto result = orbis::dmem::query(args.devIndex, args.offset, queryFlags);

  if (!result) {
    return orbis::ErrorCode::ACCES;
  }

  DirectMemoryQueryInfo info{
      .start = result->range.beginAddress(),
      .end = result->range.endAddress(),
      .memoryType = result->memoryType,
  };

  ORBIS_LOG_WARNING(__FUNCTION__, device->index, args.devIndex, args.unk,
                    args.flags, args.offset, args.info, args.infoSize,
                    info.start, info.end, (int)info.memoryType);

  return orbis::uwrite(args.info, info);
}

static orbis::ErrorCode
dmem_ioctl_checked_release(orbis::Thread *thread, DmemDevice *device,
                           const DmemIoctlRelease &args) {
  return dmem_ioctl_release(thread, device, args);
}

static orbis::ErrorCode dmem_ioctl_get_avail_size(orbis::Thread *thread,
                                                  DmemDevice *device,
                                                  DmemIoctlGetAvailSize &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.searchStart, args.searchEnd,
                    args.alignment, args.size);

  auto [range, errc] = orbis::dmem::getAvailSize(
      device->index,
      rx::AddressRange::fromBeginEnd(args.searchStart, args.searchEnd),
      args.alignment);

  if (errc != orbis::ErrorCode{}) {
    return errc;
  }

  ORBIS_LOG_WARNING(__FUNCTION__, args.searchStart, args.searchEnd,
                    args.alignment, args.size, range.beginAddress(),
                    range.size());

  args.searchStart = range.beginAddress();
  args.size = range.size();
  return {};
}

static orbis::ErrorCode dmem_ioctl_reserve(orbis::Thread *thread,
                                           DmemDevice *device,
                                           DmemIoctlReserve &args) {
  ORBIS_LOG_WARNING(__FUNCTION__, args.size, (int)args.flags);
  auto [offset, errc] = orbis::dmem::reserveSystem(device->index, args.size);

  if (errc == orbis::ErrorCode{}) {
    args.size = offset;
  }
  return errc;
}

DmemDevice::DmemDevice(int index) : index(index) {
  blockFlags = orbis::vmem::BlockFlags::DirectMemory;

  addIoctl<DMEM_IOCTL_ALLOCATE>(dmem_ioctl_allocate);
  addIoctl<DMEM_IOCTL_RELEASE>(dmem_ioctl_release);
  addIoctl<DMEM_IOCTL_SET_TYPE>(dmem_ioctl_set_type);
  addIoctl<DMEM_IOCTL_GET_TYPE>(dmem_ioctl_get_type);
  addIoctl<DMEM_IOCTL_GET_TOTAL_SIZE>(dmem_ioctl_get_total_size);
  addIoctl<DMEM_IOCTL_CLEAR>(dmem_ioctl_clear);
  addIoctl<DMEM_IOCTL_TRANSFER_BUDGET>(dmem_ioctl_transfer_budget);
  addIoctl<DMEM_IOCTL_CONTROL_RELEASE>(dmem_ioctl_control_release);
  addIoctl<DMEM_IOCTL_SET_PID_AND_PROTECT>(dmem_ioctl_set_pid_and_protect);
  addIoctl<DMEM_IOCTL_ALLOCATE_FOR_MINI_APP>(dmem_ioctl_allocate_for_mini_app);
  addIoctl<DMEM_IOCTL_ALLOCATE_MAIN>(dmem_ioctl_allocate_main);
  addIoctl<DMEM_IOCTL_QUERY>(dmem_ioctl_query);
  addIoctl<DMEM_IOCTL_CHECKED_RELEASE>(dmem_ioctl_checked_release);
  addIoctl<DMEM_IOCTL_GET_AVAIL_SIZE>(dmem_ioctl_get_avail_size);
  addIoctl<DMEM_IOCTL_RESERVE>(dmem_ioctl_reserve);
}

orbis::ErrorCode
DmemDevice::map(rx::AddressRange range, std::int64_t offset,
                rx::EnumBitSet<orbis::vmem::Protection> protection,
                orbis::File *, orbis::Process *process) {
  return orbis::dmem::map(process, index, range, offset, protection);
}

static const orbis::FileOps ops = {};

orbis::ErrorCode DmemDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                  std::uint32_t flags, std::uint32_t mode,
                                  orbis::Thread *thread) {
  auto newFile = orbis::knew<DmemFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::IoDevice *createDmemCharacterDevice(int index) {
  auto *newDevice = orbis::knew<DmemDevice>(index);
  return newDevice;
}
