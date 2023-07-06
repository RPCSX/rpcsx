#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "vm.hpp"
#include <cinttypes>
#include <cstdio>

struct HidDevice : public IoDevice {};
struct HidInstance : public IoDeviceInstance {};

static std::int64_t hid_instance_ioctl(IoDeviceInstance *instance,
                                       std::uint64_t request, void *argp) {
  std::fprintf(stderr, "***ERROR*** Unhandled hid ioctl %" PRIx64 "\n",
               request);

  // 0x800c4802
  return 0;
}

static void *hid_instance_mmap(IoDeviceInstance *instance, void *address,
                               std::uint64_t size, std::int32_t prot,
                               std::int32_t flags, std::int64_t offset) {
  std::fprintf(stderr, "***ERROR*** Unhandled hid mmap %" PRIx64 "\n", offset);
  return rx::vm::map(address, size, prot, flags);
}

static std::int32_t hid_device_open(IoDevice *device,
                                    orbis::Ref<IoDeviceInstance> *instance,
                                    const char *path, std::uint32_t flags,
                                    std::uint32_t mode) {
  auto *newInstance = orbis::knew<HidInstance>();
  newInstance->ioctl = hid_instance_ioctl;
  newInstance->mmap = hid_instance_mmap;

  io_device_instance_init(device, newInstance);
  *instance = newInstance;
  return 0;
}

IoDevice *createHidCharacterDevice() {
  auto *newDevice = orbis::knew<HidDevice>();
  newDevice->open = hid_device_open;
  return newDevice;
}
