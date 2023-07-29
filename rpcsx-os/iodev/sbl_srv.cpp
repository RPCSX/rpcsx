#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "vm.hpp"
#include <cstdio>

struct SblSrvFile : public orbis::File {};

struct SblSrvDevice : IoDevice {
  orbis::shared_mutex mtx;
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode sbl_srv_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled sbl_srv ioctl", request);
  thread->where();
  return {};
}

static orbis::ErrorCode sbl_srv_mmap(orbis::File *file, void **address,
                                     std::uint64_t size, std::int32_t prot,
                                     std::int32_t flags, std::int64_t offset,
                                     orbis::Thread *thread) {
  ORBIS_LOG_FATAL("sbl_srv mmap", address, size, offset);
  auto result = rx::vm::map(*address, size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = sbl_srv_ioctl,
    .mmap = sbl_srv_mmap,
};

orbis::ErrorCode SblSrvDevice::open(orbis::Ref<orbis::File> *file,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<SblSrvFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

IoDevice *createSblSrvCharacterDevice() { return orbis::knew<SblSrvDevice>(); }
