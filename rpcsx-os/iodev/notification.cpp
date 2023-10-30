#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct NotificationFile : orbis::File {};
struct NotificationDevice : IoDevice {
  int index;

  NotificationDevice(int index) : index(index) {}
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode notification_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled notification ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = notification_ioctl,
};

orbis::ErrorCode NotificationDevice::open(orbis::Ref<orbis::File> *file, const char *path,
                      std::uint32_t flags, std::uint32_t mode,
                      orbis::Thread *thread) {
  auto newFile = orbis::knew<NotificationFile>();
  newFile->ops = &fileOps;
  newFile->device = this;

  *file = newFile;
  return {};
}

IoDevice *createNotificationCharacterDevice(int index) { return orbis::knew<NotificationDevice>(index); }
