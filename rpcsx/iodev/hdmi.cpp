#include "io-device.hpp"
#include "orbis-config.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct HDMIFile : orbis::File {};

enum {
  SCE_HDMI_IOCTL_IC_INIT = 0x20008d01,
  SCE_HDMI_IOCTL_GET_HDMI_CONFIG = 0xc0108d10,
  SCE_HDMI_IOCTL_GET_DP_STATE = 0xc0108d1a,
  SCE_HDMI_IOCTL_VIDEO_CONFIG = 0xc0148d02,
  SCE_HDMI_IOCTL_AUDIO_CONFIG = 0xc0148d03,
  SCE_HDMI_IOCTL_CONTROL_HMDVIEW_MODE = 0xc0048d11,
  SCE_HDMI_IOCTL_AUDIO_COPY_CONTROL = 0xc01c8d06,
  SCE_HDMI_IOCTL_CONTROL_AVOUT = 0xc0048d08,
  SCE_HDMI_IOCTL_AUDIO_ASP = 0xc0048d18,
  SCE_HDMI_IOCTL_AUDIO_MUTE = 0xc0048d05,
  SCE_HDMI_IOCTL_GET_HDMI_STATE = 0xc0088d0c,
  SCE_HDMI_IOCTL_CSC_DIRECT = 0xc0068d09,
};

static orbis::ErrorCode hdmi_ioctl(orbis::File *file, std::uint64_t request,
                                   void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled hdmi ioctl", request);
  thread->where();

  switch (request) {
  case SCE_HDMI_IOCTL_GET_HDMI_CONFIG: {
    struct Args {
      orbis::ptr<orbis::uint8_t> p0;
      orbis::ptr<orbis::uint8_t> p1;
    };

    break;
  }
  case SCE_HDMI_IOCTL_GET_HDMI_STATE:
    break;

  case 0xc0088d0b:
    // SNYFD01
    const char edid_1080p_5994[] =
        "\x00\xff\xff\xff\xff\xff\xff\x00\x4d\xd9\x01\xfd\x01\x01\x01\x01"
        "\x01\x14\x01\x03\x80\x90\x51\x78\x0a\x0d\xc9\xa0\x57\x47\x98\x27"
        "\x12\x48\x4c\x21\x08\x00\x81\x80\x01\x01\x01\x01\x01\x01\x01\x01"
        "\x01\x01\x01\x01\x01\x01\x02\x3a\x80\x18\x71\x38\x2d\x40\x58\x2c"
        "\x45\x00\xa0\x2a\x53\x00\x00\x1e\x01\x1d\x00\x72\x51\xd0\x1e\x20"
        "\x6e\x28\x55\x00\xa0\x2a\x53\x00\x00\x1e\x00\x00\x00\xfd\x00\x3a"
        "\x3e\x0f\x46\x0f\x00\x0a\x20\x20\x20\x20\x20\x20\x00\x00\x00\xfc"
        "\x00\x53\x4f\x4e\x59\x20\x54\x56\x0a\x20\x20\x20\x20\x20\x01\xd1"
        "\x02\x03\x34\xf0\x4a\x10\x04\x05\x03\x02\x07\x06\x20\x01\x3c\x26"
        "\x09\x07\x07\x15\x07\x50\x83\x01\x00\x00\x76\x03\x0c\x00\x10\x00"
        "\xb8\x2d\x2f\x88\x0c\x20\x90\x08\x10\x18\x10\x28\x10\x78\x10\x06"
        "\x26\xe2\x00\x7b\x01\x1d\x80\x18\x71\x1c\x16\x20\x58\x2c\x25\x00"
        "\xa0\x2a\x53\x00\x00\x9e\x8c\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e"
        "\x96\x00\xa0\x2a\x53\x00\x00\x18\x8c\x0a\xd0\x8a\x20\xe0\x2d\x10"
        "\x10\x3e\x96\x00\x38\x2a\x43\x00\x00\x18\x8c\x0a\xa0\x14\x51\xf0"
        "\x16\x00\x26\x7c\x43\x00\x38\x2a\x43\x00\x00\x98\x00\x00\x00\xae";

    struct Args {
      orbis::ptr<std::byte> edid;
    };
    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_NOTICE("0xc0088d0b", args->edid);
    std::memcpy(args->edid, edid_1080p_5994, 256);
    break;
  }
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = hdmi_ioctl,
};

struct HDMIDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<HDMIFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createHDMICharacterDevice() { return orbis::knew<HDMIDevice>(); }
