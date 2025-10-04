#pragma once

#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "rx/Rc.hpp"
#include <cstdint>
#include <system_error>

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

struct IoDevice : rx::RcBase {
  virtual orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                                std::uint32_t flags, std::uint32_t mode,
                                orbis::Thread *thread) = 0;
  virtual orbis::ErrorCode unlink(const char *path, bool recursive,
                                  orbis::Thread *thread) {
    return orbis::ErrorCode::NOTSUP;
  }
  virtual orbis::ErrorCode createSymlink(const char *target,
                                         const char *linkPath,
                                         orbis::Thread *thread) {
    return orbis::ErrorCode::NOTSUP;
  }
  virtual orbis::ErrorCode mkdir(const char *path, int mode,
                                 orbis::Thread *thread) {
    return orbis::ErrorCode::NOTSUP;
  }
  virtual orbis::ErrorCode rmdir(const char *path, orbis::Thread *thread) {
    return orbis::ErrorCode::NOTSUP;
  }
  virtual orbis::ErrorCode rename(const char *from, const char *to,
                                  orbis::Thread *thread) {
    return orbis::ErrorCode::NOTSUP;
  }
};

struct HostFsDevice : IoDevice {
  orbis::kstring hostPath;
  orbis::kstring virtualPath;

  HostFsDevice(orbis::kstring path, orbis::kstring virtualPath)
      : hostPath(std::move(path)), virtualPath(std::move(virtualPath)) {}
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
  orbis::ErrorCode unlink(const char *path, bool recursive,
                          orbis::Thread *thread) override;
  orbis::ErrorCode createSymlink(const char *target, const char *linkPath,
                                 orbis::Thread *thread) override;
  orbis::ErrorCode mkdir(const char *path, int mode,
                         orbis::Thread *thread) override;
  orbis::ErrorCode rmdir(const char *path, orbis::Thread *thread) override;
  orbis::ErrorCode rename(const char *from, const char *to,
                          orbis::Thread *thread) override;
};

orbis::ErrorCode convertErrorCode(const std::error_code &code);
orbis::ErrorCode convertErrno();
IoDevice *createHostIoDevice(orbis::kstring hostPath,
                             orbis::kstring virtualPath);
rx::Ref<orbis::File> wrapSocket(int hostFd, orbis::kstring name, int dom,
                                int type, int prot);
orbis::ErrorCode createSocket(rx::Ref<orbis::File> *file, orbis::kstring name,
                              int dom, int type, int prot);
orbis::File *createHostFile(int hostFd, rx::Ref<IoDevice> device,
                            bool alignTruncate = false);
IoDevice *createFdWrapDevice(int fd);
