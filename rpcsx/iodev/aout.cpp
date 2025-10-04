#include "audio/AudioDevice.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/ProcessOps.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/Rc.hpp"
#include <bits/types/struct_iovec.h>
// #include <rx/hexdump.hpp>

#define SNDCTL_DSP_RESET 0x20005000
#define SNDCTL_DSP_SETFRAGMENT 0xc004500a
#define SNDCTL_DSP_SETFMT 0xc0045005
#define SNDCTL_DSP_SPEED 0xc0045002
#define SNDCTL_DSP_CHANNELS 0xc0045006
#define ORBIS_AUDIO_UPDATE_TICK_PARAMS 0xc004505c
#define SNDCTL_DSP_SYNCGROUP 0xc048501c
#define ORBIS_AUDIO_CONFIG_SPDIF 0xc0085063
#define SNDCTL_DSP_GETBLKSIZE 0x40045004
#define SOUND_PCM_READ_BITS 0x40045005
#define SNDCTL_DSP_GETOSPACE 0x4010500c
#define SNDCTL_DSP_SYNCSTART 0x8004501d
#define ORBIS_AUDIO_IOCTL_SETCONTROL 0x80085062

struct AoutFile : orbis::File {};

struct AoutDevice : public IoDevice {
  std::int8_t id;
  rx::Ref<AudioDevice> audioDevice;

  AoutDevice(std::int8_t id, AudioDevice *audioDevice)
      : id(id), audioDevice(audioDevice) {}

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static orbis::ErrorCode aout_ioctl(orbis::File *file, std::uint64_t request,
                                   void *argp, orbis::Thread *thread) {
  auto device = static_cast<AoutDevice *>(file->device.get());
  switch (request) {
  case SNDCTL_DSP_RESET: {
    ORBIS_LOG_TODO("SNDCTL_DSP_RESET");
    if (auto audioDevice = device->audioDevice) {
      audioDevice->reset();
    }
    return {};
  }
  case SNDCTL_DSP_SETFRAGMENT: {
    struct Args {
      orbis::uint32_t fragment;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_NOTICE("SNDCTL_DSP_SETFRAGMENT", args->fragment & 0xF,
                     (args->fragment >> 16) & 0xF);
    if (auto audioDevice = device->audioDevice) {
      audioDevice->setSampleSize(1 << (args->fragment & 0xF),
                                 (args->fragment >> 16) & 0xF);
    }
    return {};
  }
  case SNDCTL_DSP_SETFMT: {
    struct Args {
      orbis::uint32_t fmt;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_NOTICE("SNDCTL_DSP_SETFMT", args->fmt);
    if (auto audioDevice = device->audioDevice) {
      audioDevice->setFormat(static_cast<AudioFormat>(args->fmt));
    }
    return {};
  }
  case SNDCTL_DSP_SPEED: {
    struct Args {
      orbis::uint32_t speed;
    };
    auto args = reinterpret_cast<Args *>(argp);
    if (auto audioDevice = device->audioDevice) {
      audioDevice->setFrequency(args->speed);
    }
    return {};
  }
  case SNDCTL_DSP_CHANNELS: {
    struct Args {
      orbis::uint32_t channels;
    };
    auto args = reinterpret_cast<Args *>(argp);
    if (auto audioDevice = device->audioDevice) {
      audioDevice->setChannels(args->channels);
    }
    return {};
  }
  case ORBIS_AUDIO_UPDATE_TICK_PARAMS: {
    struct Args {
      orbis::uint32_t tick;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_NOTICE("ORBIS_AUDIO_UPDATE_TICK_PARAMS", args->tick);
    return {};
  }
  case SNDCTL_DSP_SYNCGROUP: {
    ORBIS_LOG_NOTICE("SNDCTL_DSP_SYNCGROUP");
    return {};
  }
  case ORBIS_AUDIO_CONFIG_SPDIF: {
    struct Args {
      orbis::uint64_t unk0;
    };
    auto args = reinterpret_cast<Args *>(argp);
    args->unk0 = 0x100000000; // Disable SPDIF output
    return {};
  }
  case SNDCTL_DSP_GETBLKSIZE: {
    struct Args {
      orbis::uint32_t blksize;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_NOTICE("SNDCTL_DSP_GETBLKSIZE", args->blksize);
    return {};
  }
  case SOUND_PCM_READ_BITS: {
    struct Args {
      orbis::uint32_t bits;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_NOTICE("SOUND_PCM_READ_BITS", args->bits);
    return {};
  }
  case SNDCTL_DSP_GETOSPACE: {

    if (auto audioDevice = device->audioDevice) {
      auto info = audioDevice->getOSpace();
      ORBIS_RET_ON_ERROR(orbis::uwrite(orbis::ptr<audio_buf_info>(argp), info));

      ORBIS_LOG_WARNING("SNDCTL_DSP_GETOSPACE", info.fragments, info.fragsize,
                        info.fragstotal, info.bytes);
    }

    return {};
  }
  case SNDCTL_DSP_SYNCSTART: {
    ORBIS_LOG_NOTICE("SNDCTL_DSP_SYNCSTART");
    if (auto audioDevice = device->audioDevice) {
      audioDevice->start();
    }
    return {};
  }
  case ORBIS_AUDIO_IOCTL_SETCONTROL: {
    struct Args {
      orbis::uint64_t unk0;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_NOTICE("ORBIS_AUDIO_IOCTL_SETCONTROL", args->unk0);
    return {};
  }
  default:
    ORBIS_LOG_FATAL("Unhandled aout ioctl", request);
    thread->where();
    break;
  }
  return {};
}

static orbis::ErrorCode aout_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *thread) {
  auto device = static_cast<AoutDevice *>(file->device.get());
  std::uint64_t totalSize = 0;
  if (auto audioDevice = device->audioDevice) {
    for (auto vec : std::span(uio->iov, uio->iovcnt)) {
      auto result = audioDevice->write(vec.base, vec.len);

      if (result < 0) {
        return static_cast<orbis::ErrorCode>(-result);
      }

      // rx::hexdump({(std::byte*)vec.base, vec.len});
      uio->offset += result;
      totalSize += result;
    }
  }

  thread->retval[0] = totalSize;
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = aout_ioctl,
    .write = aout_write,
};

orbis::ErrorCode AoutDevice::open(rx::Ref<orbis::File> *file, const char *path,
                                  std::uint32_t flags, std::uint32_t mode,
                                  orbis::Thread *thread) {
  ORBIS_LOG_FATAL("aout device open", path, flags, mode);
  auto newFile = orbis::knew<AoutFile>();
  newFile->ops = &fileOps;
  newFile->device = this;
  thread->where();

  *file = newFile;
  return {};
}

IoDevice *createAoutCharacterDevice(std::int8_t id, AudioDevice *device) {
  return orbis::knew<AoutDevice>(id, device);
}
