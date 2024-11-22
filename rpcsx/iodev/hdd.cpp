#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/uio.hpp"
#include "orbis/utils/Logs.hpp"
#include <span>

struct HddFile : orbis::File {};

struct HddDevice : IoDevice {
  std::uint64_t size;
  HddDevice(std::uint64_t size) : size(size) {}

  orbis::ErrorCode open(orbis::Ref<orbis::File> *fs, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

static_assert(0x120 - 24 == 0x108);
static orbis::ErrorCode hdd_ioctl(orbis::File *fs, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  auto device = fs->device.cast<HddDevice>();
  if (request == 0x40046480) { // DIOCGSECTORSIZE
    return orbis::uwrite(orbis::ptr<orbis::uint>(argp), 0x1000u);
  }

  if (request == 0x40086481) { // hdd size
    if (device->size == 0) {
      ORBIS_LOG_FATAL("Unknown hdd size request", request);
      thread->where();
    }
    return orbis::uwrite(orbis::ptr<orbis::ulong>(argp), device->size);
  }

  if (request == 0xc03861a1) {
    struct Args {
      orbis::uint64_t unk0; // op id?
      orbis::uint64_t unk1;
      orbis::uint64_t lbn;
      orbis::ptr<orbis::uint64_t> result;
      orbis::uint64_t unk4;
      orbis::uint64_t unk5;
      orbis::uint64_t unk6;
    };

    static_assert(sizeof(Args) == 56);

    auto args = reinterpret_cast<Args *>(argp);

    ORBIS_LOG_WARNING("hdd: a53io: read bfs block", request, args->unk0,
                      args->unk1, args->lbn, args->result, args->unk4,
                      args->unk5, args->unk6);

    if (args->unk1 == 0x700000000) {
      ORBIS_RET_ON_ERROR(
          orbis::uwrite<std::uint64_t>(args->result, 0x10f50bf520180705));
      auto superblock = (orbis::uint16_t *)(args->result + 8);
      superblock[0] = 1; // block exists
      superblock[1] = 4; // block size
    } else {
      // ORBIS_RET_ON_ERROR(
      //     orbis::uwrite<std::uint64_t>(args->result, 0x20f50bf520180713));
      ORBIS_RET_ON_ERROR(
          orbis::uwrite<std::uint64_t>(args->result, 0x20f50bf520190705));
      auto flags = (orbis::uint8_t *)(args->result + 2);
      *flags = 0x40; // block is clean

      // FIXME: it should be configurable
      std::strcpy((char *)(args->result + 4), "ssd0.user");
    }
    return {};
  }

  if (request == 0x4100bf0a) {
    ORBIS_LOG_WARNING("hdd: bfs scfstat");

    struct Args {
      std::uint64_t data[32];
    };

    static_assert(sizeof(Args) == 256);

    auto args = reinterpret_cast<Args *>(argp);
    std::memset(args, 0, sizeof(Args));
    // 0xe
    args->data[0] = 1ull << 32;
    args->data[1] = 1ull << 32;
    args->data[16] = 1ull << 15;

    *(orbis::uint16_t *)((char *)args->data + 200) = 1;
    // block is clean
    return {};
  }

  ORBIS_LOG_FATAL("Unhandled hdd ioctl", request);
  thread->where();
  return {};
}

static orbis::ErrorCode hdd_read(orbis::File *file, orbis::Uio *uio,
                                 orbis::Thread *thread) {
  auto dev = file->device.get();

  ORBIS_LOG_ERROR(__FUNCTION__, uio->offset);
  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    std::memset(vec.base, 0, vec.len);

    // HACK: dummy UFS header
    if (uio->offset == 0x10000) {
      *(uint32_t *)((char *)vec.base + 0x55c) = 0x19540119;
      *(uint64_t *)((char *)vec.base + 0x3e8) = 0x1000;
      *(uint8_t *)((char *)vec.base + 0xd3) = 1;
      *(uint8_t *)((char *)vec.base + 0xd1) = 1;
    }
    uio->offset += vec.len;
  }
  return {};
}

static orbis::ErrorCode hdd_stat(orbis::File *fs, orbis::Stat *sb,
                                 orbis::Thread *thread) {
  // TODO
  ORBIS_LOG_ERROR(__FUNCTION__);
  *sb = {};
  sb->mode = 0x2000;
  return {};
}

static const orbis::FileOps fsOps = {
    .ioctl = hdd_ioctl,
    .read = hdd_read,
    .stat = hdd_stat,
};

orbis::ErrorCode HddDevice::open(orbis::Ref<orbis::File> *fs, const char *path,
                                 std::uint32_t flags, std::uint32_t mode,
                                 orbis::Thread *thread) {
  auto newFile = orbis::knew<HddFile>();
  newFile->ops = &fsOps;
  newFile->device = this;

  *fs = newFile;
  return {};
}

IoDevice *createHddCharacterDevice(std::uint64_t size) {
  return orbis::knew<HddDevice>(size);
}
