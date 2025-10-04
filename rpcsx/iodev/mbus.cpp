#include "mbus.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"

struct MBusFile : orbis::File {};

static orbis::ErrorCode mbus_ioctl(orbis::File *file, std::uint64_t request,
                                   void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled mbus ioctl", request);
  return {};
}

static orbis::ErrorCode mbus_read(orbis::File *file, orbis::Uio *uio,
                                  orbis::Thread *thread) {
  auto mbus = file->device.staticCast<MBusDevice>();
  ORBIS_LOG_ERROR(__FUNCTION__);

  MBusEvent event;
  {
    std::lock_guard lock(mbus->mtx);

    // while (mbus->events.empty()) {
    //   file->mtx.unlock();
    //   mbus->cv.wait(mbusAv->mtx);
    //   file->mtx.lock();
    // }

    if (mbus->events.empty()) {
      return orbis::ErrorCode::BUSY;
    }

    event = mbus->events.front();
    mbus->events.pop_front();
  }

  return uio->write(event);
}

static const orbis::FileOps fileOps = {
    .ioctl = mbus_ioctl,
    .read = mbus_read,
};

void MBusDevice::emitEvent(const MBusEvent &event) {
  std::lock_guard lock(mtx);
  events.push_back(event);
  cv.notify_one(mtx);
  eventEmitter->emit(orbis::kEvFiltRead);
}

orbis::ErrorCode MBusDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                  std::uint32_t flags, std::uint32_t mode,
                                  orbis::Thread *thread) {
  ORBIS_LOG_FATAL("mbus device open");
  auto newFile = orbis::knew<MBusFile>();
  newFile->ops = &fileOps;
  newFile->device = this;
  newFile->event = eventEmitter;

  *file = newFile;
  return {};
}

IoDevice *createMBusCharacterDevice() { return orbis::knew<MBusDevice>(); }
