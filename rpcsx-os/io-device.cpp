#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include <fcntl.h>
#include <string>
#include <unistd.h>

std::int64_t io_device_instance_close(IoDeviceInstance *instance) { return 0; }

void io_device_instance_init(IoDevice *device, IoDeviceInstance *instance) {
  if (instance->device == nullptr) {
    instance->device = device;
  }

  if (instance->close == nullptr) {
    instance->close = io_device_instance_close;
  }
}

struct HostIoDevice : IoDevice {
  orbis::utils::kstring hostPath;
};

struct HostIoDeviceInstance : IoDeviceInstance {
  int hostFd;
};

static std::int64_t host_io_device_instance_read(IoDeviceInstance *instance,
                                                 void *data,
                                                 std::uint64_t size) {
  auto hostIoInstance = static_cast<HostIoDeviceInstance *>(instance);
  return ::read(hostIoInstance->hostFd, data, size); // TODO: convert errno
}

static std::int64_t host_io_device_instance_write(IoDeviceInstance *instance,
                                                  const void *data,
                                                  std::uint64_t size) {
  auto hostIoInstance = static_cast<HostIoDeviceInstance *>(instance);
  return ::write(hostIoInstance->hostFd, data, size); // TODO: convert errno
}

static std::int64_t host_io_device_instance_lseek(IoDeviceInstance *instance,
                                                  std::uint64_t offset,
                                                  std::uint32_t whence) {
  auto hostIoInstance = static_cast<HostIoDeviceInstance *>(instance);

  return ::lseek(hostIoInstance->hostFd, offset, whence); // TODO: convert errno
}

static std::int64_t host_io_device_instance_close(IoDeviceInstance *instance) {
  auto hostIoInstance = static_cast<HostIoDeviceInstance *>(instance);
  ::close(hostIoInstance->hostFd);
  return io_device_instance_close(instance);
}

static std::int32_t host_io_open(IoDevice *device,
                                 orbis::Ref<IoDeviceInstance> *instance,
                                 const char *path, std::uint32_t flags,
                                 std::uint32_t mode) {
  auto hostDevice = static_cast<HostIoDevice *>(device);
  auto realPath = hostDevice->hostPath + "/" + path;

  int realFlags = flags & O_ACCMODE;
  flags &= ~O_ACCMODE;

  if ((flags & kOpenFlagAppend) != 0) {
    realFlags |= O_APPEND;
    flags &= ~kOpenFlagAppend;
  }

  if ((flags & kOpenFlagNonBlock) != 0) {
    realFlags |= O_NONBLOCK;
    flags &= ~kOpenFlagNonBlock;
  }

  if ((flags & kOpenFlagFsync) != 0) {
    realFlags |= O_FSYNC;
    flags &= ~kOpenFlagFsync;
  }

  if ((flags & kOpenFlagAsync) != 0) {
    realFlags |= O_ASYNC;
    flags &= ~kOpenFlagAsync;
  }

  if ((flags & kOpenFlagTrunc) != 0) {
    realFlags |= O_TRUNC;
    flags &= ~kOpenFlagTrunc;
  }

  if ((flags & kOpenFlagCreat) != 0) {
    realFlags |= O_CREAT;
    flags &= ~kOpenFlagCreat;
  }

  if ((flags & kOpenFlagExcl) != 0) {
    realFlags |= O_EXCL;
    flags &= ~kOpenFlagExcl;
  }

  if (flags != 0) {
    std::fprintf(stderr, "host_io_open: ***ERROR*** Unhandled open flags %x\n",
                 flags);
  }

  int hostFd = ::open(realPath.c_str(), realFlags, 0777);

  if (hostFd < 0) {
    std::fprintf(stderr, "host_io_open: '%s' not found.\n", realPath.c_str());
    return 1; // TODO: convert errno
  }

  auto newInstance = orbis::knew<HostIoDeviceInstance>();

  newInstance->hostFd = hostFd;

  newInstance->read = host_io_device_instance_read;
  newInstance->write = host_io_device_instance_write;
  newInstance->lseek = host_io_device_instance_lseek;
  newInstance->close = host_io_device_instance_close;

  io_device_instance_init(device, newInstance);

  *instance = newInstance;
  return 0;
}

IoDevice *createHostIoDevice(const char *hostPath) {
  auto result = orbis::knew<HostIoDevice>();
  result->open = host_io_open;
  result->hostPath = hostPath;
  return result;
}
