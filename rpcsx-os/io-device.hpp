#pragma once

#include "orbis/utils/Rc.hpp"
#include <cstdint>

struct IoDevice;

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
};

struct IoDeviceInstance : orbis::RcBase {
  orbis::Ref<IoDevice> device;

  std::int64_t (*ioctl)(IoDeviceInstance *instance, std::uint64_t request,
                        void *argp) = nullptr;

  std::int64_t (*write)(IoDeviceInstance *instance, const void *data,
                        std::uint64_t size) = nullptr;
  std::int64_t (*read)(IoDeviceInstance *instance, void *data,
                       std::uint64_t size) = nullptr;
  std::int64_t (*close)(IoDeviceInstance *instance) = nullptr;
  std::int64_t (*lseek)(IoDeviceInstance *instance, std::uint64_t offset,
                        std::uint32_t whence) = nullptr;

  void *(*mmap)(IoDeviceInstance *instance, void *address, std::uint64_t size,
                std::int32_t prot, std::int32_t flags,
                std::int64_t offset) = nullptr;
  void *(*munmap)(IoDeviceInstance *instance, void *address,
                  std::uint64_t size) = nullptr;
};

struct IoDevice : orbis::RcBase {
  std::int32_t (*open)(IoDevice *device, orbis::Ref<IoDeviceInstance> *instance,
                       const char *path, std::uint32_t flags,
                       std::uint32_t mode) = nullptr;
};


std::int64_t io_device_instance_close(IoDeviceInstance *instance);
void io_device_instance_init(IoDevice *device, IoDeviceInstance *instance);

IoDevice *createHostIoDevice(const char *hostPath);
