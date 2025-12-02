#include "KernelContext.hpp"
#include "error.hpp"
#include "rx/AddressRange.hpp"
#include "rx/align.hpp"
#include "rx/format.hpp"
#include "rx/print.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/ProcessOps.hpp"
#include "thread/Thread.hpp"
#include "utils/Logs.hpp"
#include "vmem.hpp"

orbis::SysResult orbis::sys_sbrk(Thread *, sint) {
  return ErrorCode::OPNOTSUPP;
}
orbis::SysResult orbis::sys_sstk(Thread *, sint) {
  return ErrorCode::OPNOTSUPP;
}

orbis::SysResult orbis::sys_mmap(Thread *thread, uintptr_t addr, size_t len,
                                 rx::EnumBitSet<vmem::Protection> prot,
                                 rx::EnumBitSet<vmem::MapFlags> flags, sint fd,
                                 off_t pos) {

  std::uint64_t callerAddress = getCallerAddress(thread);

  auto shift = addr & (vmem::kPageSize - 1);

  if ((flags & vmem::MapFlags::Fixed) && shift != 0) {
    return ErrorCode::INVAL;
  }

  if (len == 0) {
    return ErrorCode::INVAL;
  }

  addr = rx::alignUp(addr, vmem::kPageSize);
  rx::EnumBitSet<AllocationFlags> allocFlags{};
  rx::EnumBitSet<vmem::BlockFlags> blockFlags{};
  rx::EnumBitSet<vmem::BlockFlagsEx> blockFlagsEx{};
  std::uint64_t alignment = vmem::kPageSize;

  {
    auto unpacked = unpackMapFlags(flags, vmem::kPageSize);
    alignment = unpacked.first;
    flags = unpacked.second;
  }

  len = rx::alignUp(len, vmem::kPageSize);

  if (flags & vmem::MapFlags::Stack) {
    allocFlags |= AllocationFlags::Stack;
  }

  if (flags & vmem::MapFlags::Fixed) {
    allocFlags |= AllocationFlags::Fixed;
  }

  if (flags & vmem::MapFlags::NoOverwrite) {
    allocFlags |= AllocationFlags::NoOverwrite;
  }

  if (flags & vmem::MapFlags::NoCoalesce) {
    allocFlags |= AllocationFlags::NoMerge;
  }

  if (flags & vmem::MapFlags::Shared) {
    blockFlagsEx |= vmem::BlockFlagsEx::Shared;

    if (flags & vmem::MapFlags::Private) {
      return ErrorCode::INVAL;
    }
  }

  if (flags & vmem::MapFlags::Private) {
    blockFlagsEx |= vmem::BlockFlagsEx::Private;
  }

  if (addr == 0) {
    addr = flags & vmem::MapFlags::System ? 0xfc0000000 : 0x200000000;
  }

  auto name = callerAddress ? rx::format("anon:{:012x}", callerAddress) : "";

  if (flags & vmem::MapFlags::Void) {
    if (fd != -1 || pos != 0) {
      return ErrorCode::INVAL;
    }

    auto [range, errc] = vmem::mapVoid(thread->tproc, len, addr, allocFlags,
                                       name, alignment, callerAddress);

    if (errc != orbis::ErrorCode{}) {
      return errc;
    }

    thread->retval[0] = range.beginAddress();
    return {};
  }

  if (flags & vmem::MapFlags::Stack) {
    flags |= vmem::MapFlags::Anon;
    blockFlags |= vmem::BlockFlags::Stack;

    if (fd != -1 || pos != 0) {
      return ErrorCode::INVAL;
    }

    if ((prot & (vmem::Protection::CpuRead | vmem::Protection::CpuWrite)) !=
        (vmem::Protection::CpuRead | vmem::Protection::CpuWrite)) {
      return ErrorCode::INVAL;
    }
  } else {
    shift = 0;
  }

  if (flags & vmem::MapFlags::Anon) {
    if (fd != -1 || pos != 0) {
      return ErrorCode::INVAL;
    }

    auto [range, errc] =
        vmem::mapFlex(thread->tproc, len, prot, addr, allocFlags, blockFlags,
                      name, alignment, callerAddress);

    if (errc != orbis::ErrorCode{}) {
      return errc;
    }

    thread->retval[0] = range.beginAddress() + shift;
    return {};
  }

  auto file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (!file->device->blockFlags) {
    blockFlags |= vmem::BlockFlags::FlexibleMemory;
    prot &= ~vmem::Protection::CpuExec;
  }

  auto [range, errc] = vmem::mapFile(thread->tproc, addr, len, allocFlags, prot,
                                     blockFlags, blockFlagsEx, file.get(), pos,
                                     name, alignment, callerAddress);

  if (errc != ErrorCode{}) {
    return errc;
  }

  thread->retval[0] = range.beginAddress() + shift;
  return {};
}

orbis::SysResult orbis::sys_freebsd6_mmap(Thread *thread, uintptr_t addr,
                                          size_t len,
                                          rx::EnumBitSet<vmem::Protection> prot,
                                          rx::EnumBitSet<vmem::MapFlags> flags,
                                          sint fd, sint, off_t pos) {
  return sys_mmap(thread, addr, len, prot, flags, fd, pos);
}
orbis::SysResult orbis::sys_msync(Thread *thread, uintptr_t addr, size_t len,
                                  sint flags) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, len, flags);
  return {};
}
orbis::SysResult orbis::sys_munmap(Thread *thread, uintptr_t addr, size_t len) {
  if (len == 0) {
    return ErrorCode::INVAL;
  }

  auto range = rx::AddressRange::fromBeginSize(addr, len);

  return vmem::unmap(thread->tproc, range);
}
orbis::SysResult orbis::sys_mprotect(Thread *thread, uintptr_t addr, size_t len,
                                     rx::EnumBitSet<vmem::Protection> prot) {
  auto range = rx::AddressRange::fromBeginSize(addr, len);
  return vmem::protect(thread->tproc, range, prot);
}
orbis::SysResult orbis::sys_minherit(Thread *thread, uintptr_t addr, size_t len,
                                     sint inherit) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, len, inherit);

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_madvise(Thread *thread, uintptr_t addr, size_t len,
                                    sint behav) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, len, behav);
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mincore(Thread *thread, uintptr_t addr, size_t len,
                                    ptr<char> vec) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, len, vec);
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mlock(Thread *thread, uintptr_t addr, size_t len) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, len);
  return {};
}
orbis::SysResult orbis::sys_mlockall(Thread *thread, sint how) {
  ORBIS_LOG_TODO(__FUNCTION__, how);
  return {};
}
orbis::SysResult orbis::sys_munlockall(Thread *thread) {
  ORBIS_LOG_TODO(__FUNCTION__);
  return {};
}
orbis::SysResult orbis::sys_munlock(Thread *thread, uintptr_t addr,
                                    size_t len) {
  ORBIS_LOG_TODO(__FUNCTION__, addr, len);
  return {};
}
