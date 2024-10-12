#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include <chrono>
#include <cstddef>
#include <mutex>
#include <span>
#include <thread>

struct NotificationFile : orbis::File {};
struct NotificationDevice : IoDevice {
  int index;
  orbis::shared_mutex mutex;
  orbis::kvector<std::byte> data;

  NotificationDevice(int index) : index(index) {}
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode notification_ioctl(orbis::File *file,
                                           std::uint64_t request, void *argp,
                                           orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled notification ioctl", request);
  return {};
}

static orbis::ErrorCode notification_read(orbis::File *file, orbis::Uio *uio,
                                          orbis::Thread *thread) {
  auto dev = dynamic_cast<NotificationDevice *>(file->device.get());

  while (true) {
    if (dev->data.empty()) {
      if (file->noBlock()) {
        return orbis::ErrorCode::WOULDBLOCK;
      }

      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::lock_guard lock(dev->mutex);

    if (dev->data.empty()) {
      continue;
    }

    for (auto vec : std::span(uio->iov, uio->iovcnt)) {
      auto size = std::min<std::size_t>(dev->data.size(), vec.len);
      uio->offset += size;
      std::memcpy(vec.base, dev->data.data(), size);

      if (dev->data.size() == size) {
        break;
      }

      std::memmove(dev->data.data(), dev->data.data() + size,
                   dev->data.size() - size);
      dev->data.resize(dev->data.size() - size);
    }

    break;
  }
  return {};
}

static orbis::ErrorCode notification_write(orbis::File *file, orbis::Uio *uio,
                                           orbis::Thread *thread) {
  auto dev = dynamic_cast<NotificationDevice *>(file->device.get());

  std::lock_guard lock(dev->mutex);

  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    auto offset = dev->data.size();
    dev->data.resize(offset + vec.len);
    std::memcpy(dev->data.data(), vec.base, vec.len);
    uio->offset += vec.len;
  }
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = notification_ioctl,
    .read = notification_read,
    .write = notification_write,
};

orbis::ErrorCode NotificationDevice::open(orbis::Ref<orbis::File> *file,
                                          const char *path, std::uint32_t flags,
                                          std::uint32_t mode,
                                          orbis::Thread *thread) {
  auto newFile = orbis::knew<NotificationFile>();
  newFile->ops = &fileOps;
  newFile->device = this;
  newFile->flags = flags;
  newFile->mode = mode;

  *file = newFile;
  return {};
}

IoDevice *createNotificationCharacterDevice(int index) {
  return orbis::knew<NotificationDevice>(index);
}
