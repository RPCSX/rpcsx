

#include "vfs.hpp"
#include "io-device.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/error/SysResult.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <string_view>

struct DevFs : IoDevice {
  std::map<std::string, orbis::Ref<IoDevice>> devices;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    if (auto it = devices.find(path); it != devices.end()) {
      return it->second->open(file, path, flags, mode, thread);
    }

    std::fprintf(stderr, "device %s not exists\n", path);
    return orbis::ErrorCode::NOENT;
  }
};

struct ProcFs : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    std::fprintf(stderr, "procfs access: %s\n", path);
    std::abort();
    return orbis::ErrorCode::NOENT;
  }
};

static orbis::shared_mutex gMountMtx;
static std::map<std::string, orbis::Ref<IoDevice>, std::greater<>> gMountsMap;
static orbis::Ref<DevFs> gDevFs;

void rx::vfs::fork() {
  std::lock_guard lock(gMountMtx);

  // NOTE: do not decrease reference counter, it managed by parent process
  auto parentDevFs = gDevFs.release();

  for (auto &mount : gMountsMap) {
    mount.second->incRef(); // increase reference for new process
  }

  gDevFs = orbis::knew<DevFs>();
  gMountsMap.emplace("/dev/", gDevFs);
  gMountsMap.emplace("/proc/", orbis::knew<ProcFs>());

  for (auto &fs : parentDevFs->devices) {
    gDevFs->devices[fs.first] = fs.second;
  }
}

void rx::vfs::initialize() {
  gDevFs = orbis::knew<DevFs>();
  gMountsMap.emplace("/dev/", gDevFs);
  gMountsMap.emplace("/proc/", orbis::knew<ProcFs>());
}

void rx::vfs::deinitialize() {
  gDevFs = nullptr;
  gMountsMap.clear();
}

void rx::vfs::addDevice(std::string name, IoDevice *device) {
  std::lock_guard lock(gMountMtx);
  gDevFs->devices[std::move(name)] = device;
}

static std::pair<orbis::Ref<IoDevice>, std::string>
get(const std::filesystem::path &guestPath) {
  std::string normalPath = std::filesystem::path(guestPath).lexically_normal();
  std::string_view path = normalPath;
  orbis::Ref<IoDevice> device;
  std::string_view prefix;

  std::lock_guard lock(gMountMtx);

  for (auto &mount : gMountsMap) {
    if (!path.starts_with(mount.first)) {
      continue;
    }

    device = mount.second;
    path.remove_prefix(mount.first.length());
    return {mount.second, std::string(path)};
  }

  return {};
}

orbis::SysResult rx::vfs::mount(const std::filesystem::path &guestPath,
                                IoDevice *dev) {
  auto mp = guestPath.lexically_normal().string();
  if (!mp.ends_with("/")) {
    mp += "/";
  }

  std::lock_guard lock(gMountMtx);

  auto [it, inserted] = gMountsMap.emplace(std::move(mp), dev);

  if (!inserted) {
    return orbis::ErrorCode::EXIST;
  }

  return {};
}

orbis::SysResult rx::vfs::open(std::string_view path, int flags, int mode,
                               orbis::Ref<orbis::File> *file,
                               orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }
  return device->open(file, devPath.c_str(), flags, mode, thread);
}

orbis::SysResult rx::vfs::mkdir(std::string_view path, int mode,
                                orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }
  return device->mkdir(devPath.c_str(), mode, thread);
}

orbis::SysResult rx::vfs::rmdir(std::string_view path, orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }
  return device->rmdir(devPath.c_str(), thread);
}

orbis::SysResult rx::vfs::rename(std::string_view from, std::string_view to,
                                 orbis::Thread *thread) {
  auto [fromDevice, fromDevPath] = get(from);
  if (fromDevice == nullptr) {
    return orbis::ErrorCode::NOENT;
  }

  auto [toDevice, toDevPath] = get(to);
  if (toDevice == nullptr) {
    return orbis::ErrorCode::NOENT;
  }

  if (fromDevice != toDevice) {
    std::fprintf(stderr, "cross fs rename operation: %s -> %s\n",
                 std::string(from).c_str(), std::string(to).c_str());
    std::abort();
  }

  return fromDevice->rename(fromDevPath.c_str(), toDevPath.c_str(), thread);
}
