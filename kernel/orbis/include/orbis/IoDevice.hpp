#pragma once

#include "error/ErrorCode.hpp"
#include "orbis-config.hpp"
#include "rx/AddressRange.hpp"
#include "rx/EnumBitSet.hpp"
#include "rx/Rc.hpp"
#include "rx/mem.hpp"
#include "vmem.hpp"
#include <bit>
#include <type_traits>

namespace orbis {
enum OpenFlags {
  kOpenFlagReadOnly = 0x0,
  kOpenFlagWriteOnly = 0x1,
  kOpenFlagReadWrite = 0x2,
  kOpenFlagNonBlock = 0x4,
  kOpenFlagAppend = 0x8,
  kOpenFlagShLock = 0x10,
  kOpenFlagExLock = 0x20,
  kOpenFlagAsync = 0x40,
  kOpenFlagFsync = 0x80,
  kOpenFlagCreat = 0x200,
  kOpenFlagTrunc = 0x400,
  kOpenFlagExcl = 0x800,
  kOpenFlagDSync = 0x1000,
  kOpenFlagDirect = 0x10000,
  kOpenFlagDirectory = 0x20000,
};

struct File;
struct Thread;
struct Process;
struct Stat;
struct StatFs;
struct IoDevice : rx::RcBase {
  rx::EnumBitSet<vmem::BlockFlags> blockFlags{};

  virtual ErrorCode open(rx::Ref<File> *file, const char *path,
                         std::uint32_t flags, std::uint32_t mode,
                         Thread *thread) = 0;

  virtual ErrorCode statfs(const char *path, StatFs *sb, Thread *thread) {
    return ErrorCode::NOTSUP;
  }

  virtual ErrorCode stat(const char *path, Stat *sb, Thread *thread) {
    return ErrorCode::NOTSUP;
  }

  virtual ErrorCode unlink(const char *path, bool recursive, Thread *thread) {
    return ErrorCode::NOTSUP;
  }

  virtual ErrorCode createSymlink(const char *target, const char *linkPath,
                                  Thread *thread) {
    return ErrorCode::NOTSUP;
  }

  virtual ErrorCode mkdir(const char *path, int mode, Thread *thread) {
    return ErrorCode::NOTSUP;
  }

  virtual ErrorCode rmdir(const char *path, Thread *thread) {
    return ErrorCode::NOTSUP;
  }

  virtual ErrorCode rename(const char *from, const char *to, Thread *thread) {
    return ErrorCode::NOTSUP;
  }

  virtual ErrorCode ioctl(std::uint64_t request, ptr<void> argp,
                          Thread *thread);

  virtual ErrorCode map(rx::AddressRange range, std::int64_t offset,
                        rx::EnumBitSet<vmem::Protection> protection, File *file,
                        Process *process);

  virtual std::pair<rx::AddressRange, orbis::MemoryType>
  getPmemRange(std::uint64_t offset, File *file) {
    return {};
  }

  [[nodiscard]] virtual std::string toString() const;
};

namespace ioctl {
inline constexpr std::uint32_t VoidBit = 0x20000000;
inline constexpr std::uint32_t OutBit = 0x40000000;
inline constexpr std::uint32_t InBit = 0x80000000;
inline constexpr std::uint32_t DirMask = VoidBit | InBit | OutBit;

constexpr bool isVoid(std::uint32_t cmd) { return (cmd & DirMask) == VoidBit; }
constexpr bool isIn(std::uint32_t cmd) { return (cmd & DirMask) == InBit; }
constexpr bool isOut(std::uint32_t cmd) { return (cmd & DirMask) == OutBit; }
constexpr bool isInOut(std::uint32_t cmd) {
  return (cmd & DirMask) == (OutBit | InBit);
}
constexpr std::uint32_t paramSize(std::uint32_t cmd) {
  return (cmd >> 16) & ((1 << 13) - 1);
}
constexpr std::uint32_t group(std::uint32_t cmd) { return (cmd >> 8) & 0xff; }
constexpr std::uint32_t id(std::uint32_t cmd) { return cmd & 0xff; }

std::string groupToString(unsigned iocGroup);
} // namespace ioctl

struct IoctlHandlerEntry;

using IoctlHandler = ErrorCode (*)(Thread *thread, orbis::ptr<void> arg,
                                   IoDevice *device, void (*impl)());

struct IoctlHandlerEntry {
  IoctlHandler handler;
  void (*impl)();
  std::uint32_t cmd;
};
template <int Group> struct IoDeviceWithIoctl : IoDevice {
  IoctlHandlerEntry ioctlTable[256]{};

  template <std::uint32_t Cmd, typename InstanceT, typename T>
    requires requires {
      requires std::is_base_of_v<IoDeviceWithIoctl, InstanceT>;
      requires ioctl::paramSize(Cmd) == sizeof(T);
      requires ioctl::group(Cmd) == Group;
      requires std::is_const_v<T> || ioctl::isInOut(Cmd);
      requires !std::is_const_v<T> || ioctl::isIn(Cmd);
    }
  void addIoctl(ErrorCode (*handler)(Thread *thread, InstanceT *device,
                                     T &arg)) {
    constexpr auto id = ioctl::id(Cmd);
    assert(ioctlTable[id].handler == nullptr);

    IoctlHandlerEntry &entry = ioctlTable[id];

    entry.handler = [](Thread *thread, orbis::ptr<void> arg, IoDevice *device,
                       void (*opaqueImpl)()) -> ErrorCode {
      auto impl = std::bit_cast<decltype(handler)>(opaqueImpl);

      std::remove_cvref_t<T> _arg;
      ORBIS_RET_ON_ERROR(orbis::uread(_arg, orbis::ptr<T>(arg)));
      ORBIS_RET_ON_ERROR(impl(thread, static_cast<InstanceT *>(device), _arg));

      if constexpr (ioctl::isInOut(Cmd)) {
        return orbis::uwrite(orbis::ptr<T>(arg), _arg);
      } else {
        return {};
      }
    };
    entry.impl = std::bit_cast<void (*)()>(handler);
    entry.cmd = Cmd;
  }

  template <std::uint32_t Cmd, typename InstanceT>
    requires requires {
      requires std::is_base_of_v<IoDeviceWithIoctl, InstanceT>;
      requires ioctl::group(Cmd) == Group;
      requires ioctl::isVoid(Cmd);
    }

  void addIoctl(ErrorCode (*handler)(Thread *thread, InstanceT *device)) {
    constexpr auto id = ioctl::id(Cmd);
    assert(ioctlTable[id].handler == nullptr);

    IoctlHandlerEntry &entry = ioctlTable[id];

    entry.handler = [](Thread *thread, orbis::ptr<void>, IoDevice *device,
                       void (*opaqueImpl)()) -> ErrorCode {
      auto impl = std::bit_cast<decltype(handler)>(opaqueImpl);
      return impl(thread, static_cast<InstanceT *>(device));
    };

    entry.impl = std::bit_cast<void (*)()>(handler);
    entry.cmd = Cmd;
  }

  template <std::uint32_t Cmd, typename InstanceT, typename T>
    requires requires {
      requires std::is_base_of_v<IoDeviceWithIoctl, InstanceT>;
      requires ioctl::paramSize(Cmd) == sizeof(T);
      requires ioctl::group(Cmd) == Group;
      requires ioctl::isOut(Cmd);
    }
  void addIoctl(std::pair<ErrorCode, T> (*handler)(Thread *thread,
                                                   InstanceT *device)) {
    constexpr auto id = ioctl::id(Cmd);
    assert(ioctlTable[id].handler == nullptr);

    IoctlHandlerEntry &entry = ioctlTable[id];

    entry.handler = [](Thread *thread, orbis::ptr<void> arg, IoDevice *device,
                       void (*opaqueImpl)()) -> ErrorCode {
      auto impl = std::bit_cast<decltype(handler)>(opaqueImpl);
      auto [errc, value] = impl(thread, static_cast<InstanceT *>(device));
      ORBIS_RET_ON_ERROR(errc);
      return orbis::uwrite(orbis::ptr<T>(arg), value);
    };
    entry.impl = std::bit_cast<void (*)()>(handler);
    entry.cmd = Cmd;
  }

  ErrorCode ioctl(std::uint64_t request, orbis::ptr<void> argp,
                  Thread *thread) override {
    auto id = request & 0xff;

    if (ioctlTable[id].handler == nullptr || ioctlTable[id].cmd != request) {
      return IoDevice::ioctl(request, argp, thread);
    }

    return ioctlTable[id].handler(thread, argp, this, ioctlTable[id].impl);
  }

  [[nodiscard]] std::string toString() const override {
    return ioctl::groupToString(Group) + " " + IoDevice::toString();
  }
};
} // namespace orbis
