#pragma once

#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "rx/Rc.hpp"
#include <cstdint>
#include <system_error>

struct HostFsDevice : orbis::IoDevice {
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
orbis::IoDevice *createHostIoDevice(orbis::kstring hostPath,
                                    orbis::kstring virtualPath);
rx::Ref<orbis::File> wrapSocket(int hostFd, orbis::kstring name, int dom,
                                int type, int prot);
orbis::ErrorCode createSocket(rx::Ref<orbis::File> *file, orbis::kstring name,
                              int dom, int type, int prot);
orbis::File *createHostFile(int hostFd, orbis::IoDevice *device,
                            bool alignTruncate = false);
orbis::IoDevice *createFdWrapDevice(int fd);
