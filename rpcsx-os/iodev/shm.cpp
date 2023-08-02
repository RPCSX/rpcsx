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
};

orbis::ErrorCode ShmDevice::open(orbis::Ref<orbis::File> *file,
                                 const char *path, std::uint32_t flags,
                                 std::uint32_t mode, orbis::Thread *thread) {
  ORBIS_LOG_WARNING("shm_open", path, flags, mode);

  std::string name = "/rpcsx-";
  if (std::string_view{path}.starts_with("/")) {
    name += path + 1;
  } else {
    name += path;
  }

  auto realFlags = O_RDWR; // TODO

  std::size_t size = 0;
  if (~flags & 0x200) {
    if (name == "/rpcsx-SceShellCoreUtil") {
      // TODO
      realFlags |= O_CREAT;
      size = 0x4000;
    } else if (name == "/rpcsx-vmicDdShmAin") {
      // TODO
      realFlags |= O_CREAT;
      size = 0x4000;
    } else if (name == "/rpcsx-SceNpPlusLogger") {
      realFlags |= O_CREAT;
      size = 0x4400;
    } else {
      ORBIS_LOG_ERROR("SHM: unknown shared memory", path);
      thread->where();
      std::abort();
    }
  } else {
    realFlags |= O_CREAT;
  }

  int fd = shm_open(name.c_str(), realFlags, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    std::printf("shm_open: error %u\n", errno);
    return orbis::ErrorCode::ACCES;
  }
  auto hostFile = createHostFile(fd, this);
  if (size != 0) {
    hostFile->ops->truncate(hostFile, size, thread);
  }

  *file = hostFile;
  return {};
}

IoDevice *createShmDevice() { return orbis::knew<ShmDevice>(); }
