#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include <fcntl.h>
#include <sys/mman.h>

struct ShmDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
  orbis::ErrorCode unlink(const char *path, bool recursive,
                          orbis::Thread *thread) override;
};

static std::string realShmPath(const char *path) {
  std::string name = "/rpcsx-";
  if (path[0] == '/') {
    name += path + 1;
  } else {
    name += path;
  }

  for (auto pos = name.find('/', 1); pos != std::string::npos;
       pos = name.find('/', pos + 1)) {
    name[pos] = '$';
  }

  return name;
}

orbis::ErrorCode ShmDevice::open(orbis::Ref<orbis::File> *file,
                                 const char *path, std::uint32_t flags,
                                 std::uint32_t mode, orbis::Thread *thread) {
  ORBIS_LOG_WARNING("shm_open", path, flags, mode);
  auto name = realShmPath(path);
  auto realFlags = O_RDWR; // TODO

  std::size_t size = 0;
  if (flags & 0x200) {
    realFlags |= O_CREAT;
  }

  int fd = shm_open(name.c_str(), realFlags, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return convertErrno();
  }
  auto hostFile = createHostFile(fd, this);
  if (size != 0) {
    hostFile->ops->truncate(hostFile, size, thread);
  }

  *file = hostFile;
  return {};
}

orbis::ErrorCode ShmDevice::unlink(const char *path, bool recursive,
                          orbis::Thread *thread) {
  ORBIS_LOG_WARNING("shm_unlink", path);
  auto name = realShmPath(path);

  if (shm_unlink(name.c_str())) {
    return convertErrno();
  }

  return{};
}
IoDevice *createShmDevice() { return orbis::knew<ShmDevice>(); }
