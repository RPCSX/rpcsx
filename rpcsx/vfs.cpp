

#include "vfs.hpp"
#include "io-device.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/error/SysResult.hpp"
#include <filesystem>
#include <map>
#include <string_view>

static orbis::ErrorCode devfs_stat(orbis::File *file, orbis::Stat *sb,
                                   orbis::Thread *thread) {
  *sb = {}; // TODO
  return {};
}

static orbis::FileOps devfs_ops = {
    .stat = devfs_stat,
};

struct DevFs : IoDevice {
  std::map<std::string, rx::Ref<IoDevice>, std::less<>> devices;

  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    if (path[0] == '\0') {
      auto result = orbis::knew<orbis::File>();
      for (auto &[name, dev] : devices) {
        auto &entry = result->dirEntries.emplace_back();
        entry.fileno = result->dirEntries.size();
        entry.reclen = sizeof(orbis::Dirent);
        entry.type = orbis::kDtBlk;
        entry.namlen = name.size();
        std::strncpy(entry.name, name.c_str(), sizeof(entry.name));
      }

      result->ops = &devfs_ops;
      *file = result;
      return {};
    }

    std::string_view devPath = path;
    if (auto it = devices.find(devPath); it != devices.end()) {
      return it->second->open(file, "", flags, mode, thread);
    }

    std::fprintf(stderr, "device %s not exists\n", path);
    return orbis::ErrorCode::NOENT;
  }
};

struct ProcFs : IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    std::fprintf(stderr, "procfs access: %s\n", path);
    return orbis::ErrorCode::NOENT;
  }
};

static rx::shared_mutex gMountMtx;
static std::map<std::string, rx::Ref<IoDevice>, std::greater<>> gMountsMap;
static rx::Ref<DevFs> gDevFs;

void vfs::fork() {
  std::lock_guard lock(gMountMtx);

  // NOTE: do not decrease reference counter, it managed by parent process
  auto parentDevFs = gDevFs.release();

  for (auto &mount : gMountsMap) {
    mount.second->incRef(); // increase reference for new process
  }

  gDevFs = orbis::knew<DevFs>();
  gMountsMap["/dev/"] = gDevFs;
  gMountsMap["/proc/"] = orbis::knew<ProcFs>();

  for (auto &fs : parentDevFs->devices) {
    gDevFs->devices[fs.first] = fs.second;
  }
}

void vfs::initialize() {
  gDevFs = orbis::knew<DevFs>();
  gMountsMap.emplace("/dev/", gDevFs);
  gMountsMap.emplace("/proc/", orbis::knew<ProcFs>());
}

void vfs::deinitialize() {
  gDevFs = nullptr;
  gMountsMap.clear();
}

void vfs::addDevice(std::string name, IoDevice *device) {
  std::lock_guard lock(gMountMtx);
  gDevFs->devices[std::move(name)] = device;
}

std::pair<rx::Ref<IoDevice>, std::string>
vfs::get(const std::filesystem::path &guestPath) {
  std::string normalPath = std::filesystem::path(guestPath).lexically_normal();
  std::string_view path = normalPath;
  rx::Ref<IoDevice> device;

  std::lock_guard lock(gMountMtx);

  if (gDevFs != nullptr) {
    std::string_view devPath = "/dev/";
    if (path.starts_with(devPath) ||
        path == devPath.substr(0, devPath.size() - 1)) {
      if (path.size() > devPath.size()) {
        path.remove_prefix(devPath.size());
      } else {
        path = {};
      }

      return {gDevFs, std::string(path)};
    }
  }

  for (auto &mount : gMountsMap) {
    if (!path.starts_with(mount.first)) {
      if (mount.first.size() - 1 != path.size() ||
          !std::string_view(mount.first).starts_with(path)) {
        continue;
      }
    }

    device = mount.second;
    if (path.size() > mount.first.length()) {
      path.remove_prefix(mount.first.length());
    } else {
      path = {};
    }
    return {mount.second, std::string(path)};
  }

  return {};
}

orbis::SysResult vfs::mount(const std::filesystem::path &guestPath,
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

orbis::SysResult vfs::open(std::string_view path, int flags, int mode,
                           rx::Ref<orbis::File> *file, orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }
  // std::fprintf(stderr, "sys_open %s\n", std::string(path).c_str());
  return device->open(file, devPath.c_str(), flags, mode, thread);
}

bool vfs::exists(std::string_view path, orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return false;
  }

  rx::Ref<orbis::File> file;
  if (device->open(&file, devPath.c_str(), 0, 0, thread) !=
      orbis::ErrorCode{}) {
    return false;
  }
  return true;
}

orbis::SysResult vfs::mkdir(std::string_view path, int mode,
                            orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }
  return device->mkdir(devPath.c_str(), mode, thread);
}

orbis::SysResult vfs::rmdir(std::string_view path, orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }
  return device->rmdir(devPath.c_str(), thread);
}

orbis::SysResult vfs::rename(std::string_view from, std::string_view to,
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

orbis::ErrorCode vfs::unlink(std::string_view path, orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }

  return device->unlink(devPath.c_str(), false, thread);
}

orbis::ErrorCode vfs::unlinkAll(std::string_view path, orbis::Thread *thread) {
  auto [device, devPath] = get(path);
  if (device == nullptr) {
    return orbis::ErrorCode::NOENT;
  }

  return device->unlink(devPath.c_str(), true, thread);
}

orbis::ErrorCode vfs::createSymlink(std::string_view target,
                                    std::string_view linkPath,
                                    orbis::Thread *thread) {
  auto [fromDevice, fromDevPath] = get(target);
  if (fromDevice == nullptr) {
    return orbis::ErrorCode::NOENT;
  }

  auto [targetDevice, toDevPath] = get(linkPath);
  if (targetDevice == nullptr) {
    return orbis::ErrorCode::NOENT;
  }

  if (fromDevice != targetDevice) {
    auto fromHost = fromDevice.cast<HostFsDevice>();
    auto targetHost = targetDevice.cast<HostFsDevice>();

    if (fromHost == nullptr || targetHost == nullptr) {
      std::fprintf(stderr, "cross fs operation: %s -> %s\n",
                   std::string(target).c_str(), std::string(linkPath).c_str());
      std::abort();
    }

    std::error_code ec;
    std::filesystem::create_symlink(
        std::filesystem::absolute(fromHost->hostPath + "/" +
                                  fromDevPath.c_str()),
        targetHost->hostPath + "/" + toDevPath.c_str(), ec);
    return convertErrorCode(ec);
  }

  return fromDevice->createSymlink(fromDevPath.c_str(), toDevPath.c_str(),
                                   thread);
}
