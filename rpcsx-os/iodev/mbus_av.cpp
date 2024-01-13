#include "mbus_av.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/note.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/SharedCV.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include <mutex>

struct MBusAVFile : orbis::File {};

static orbis::ErrorCode mbus_av_ioctl(orbis::File *file, std::uint64_t request,
                                      void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled mbus_av ioctl", request);
  return {};
}

static orbis::ErrorCode mbus_av_read(orbis::File *file, orbis::Uio *uio,
                                     orbis::Thread *thread) {
  auto mbusAv = file->device.staticCast<MBusAVDevice>();

  MBusEvent event;
  {
    std::lock_guard lock(mbusAv->mtx);

    // while (mbusAv->events.empty()) {
    //   file->mtx.unlock();
    //   mbusAv->cv.wait(mbusAv->mtx);
    //   file->mtx.lock();
    // }

    if (mbusAv->events.empty()) {
      return orbis::ErrorCode::BUSY;
    }

    event = mbusAv->events.front();
    mbusAv->events.pop_front();
  }

  return uio->write(event);
}

static const orbis::FileOps fileOps = {
    .ioctl = mbus_av_ioctl,
    .read = mbus_av_read,
};

orbis::ErrorCode MBusAVDevice::open(orbis::Ref<orbis::File> *file,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<MBusAVFile>();
  newFile->ops = &fileOps;
  newFile->device = this;
  newFile->event = eventEmitter;

  *file = newFile;
  return {};
}

void MBusAVDevice::emitEvent(const MBusEvent &event) {
  std::lock_guard lock(mtx);
  events.push_back(event);
  cv.notify_one(mtx);

  eventEmitter->emit(orbis::kEvFiltRead);
}

IoDevice *createMBusAVCharacterDevice() { return orbis::knew<MBusAVDevice>(); }
