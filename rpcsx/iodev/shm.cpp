#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/watchdog.hpp"
#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>

struct ShmDevice : IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
  orbis::ErrorCode unlink(const char *path, bool recursive,
                          orbis::Thread *thread) override;
};

orbis::ErrorCode ShmDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                 std::uint32_t flags, std::uint32_t mode,
                                 orbis::Thread *thread) {
  ORBIS_LOG_WARNING("shm_open", path, flags, mode);
  auto hostPath = rx::getShmGuestPath(path);
  auto realFlags = O_RDWR; // TODO

  if (flags & 0x200) {
    realFlags |= O_CREAT;
  }

  std::filesystem::create_directories(hostPath.parent_path());

  int fd = ::open(hostPath.c_str(), realFlags, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return convertErrno();
  }

  auto hostFile = createHostFile(fd, this, true);
  *file = hostFile;
  return {};
}

orbis::ErrorCode ShmDevice::unlink(const char *path, bool recursive,
                                   orbis::Thread *thread) {
  ORBIS_LOG_WARNING("shm_unlink", path);
  auto hostPath = rx::getShmGuestPath(path);

  std::error_code ec;

  if (recursive) {
    std::filesystem::remove_all(hostPath, ec);
  } else {
    std::filesystem::remove(hostPath, ec);
  }

  return convertErrorCode(ec);
}

IoDevice *createShmDevice() { return orbis::knew<ShmDevice>(); }
