

#include "vfs.hpp"
#include "io-device.hpp"
#include "orbis/error/ErrorCode.hpp"
#include "orbis/error/SysResult.hpp"
#include <filesystem>
#include <map>
#include <string_view>

static std::map<std::string, orbis::Ref<IoDevice>> sMountsMap;

void rx::vfs::initialize() {}
void rx::vfs::deinitialize() { sMountsMap.clear(); }

orbis::SysResult rx::vfs::mount(const std::filesystem::path &guestPath,
                                IoDevice *dev) {
  auto [it, inserted] =
      sMountsMap.emplace(guestPath.lexically_normal().string(), dev);

  if (!inserted) {
    return orbis::ErrorCode::EXIST;
  }

  return {};
}

orbis::SysResult rx::vfs::open(std::string_view path, int flags, int mode,
                               orbis::Ref<orbis::File> *file,
                               orbis::Thread *thread) {
  orbis::Ref<IoDevice> device;
  bool isCharacterDevice = path.starts_with("/dev/");

  for (auto &mount : sMountsMap) {
    if (!path.starts_with(mount.first)) {
      continue;
    }

    path.remove_prefix(mount.first.length());
    device = mount.second;
    break;
  }

  if (isCharacterDevice && device != nullptr) {
    if (!path.empty()) {
      std::fprintf(stderr,
                   "vfs::open: access to character device subentry '%s' (%s)\n",
                   path.data(), std::string(path).c_str());
      return orbis::ErrorCode::NOENT;
    }
  }

  if (device != nullptr) {
    return (orbis::ErrorCode)device->open(file, std::string(path).c_str(),
                                          flags, mode, thread);
  }

  if (isCharacterDevice) {
    std::fprintf(stderr, "vfs::open: character device '%s' not found.\n",
                 std::string(path).c_str());
  }

  return orbis::ErrorCode::NOENT;
}
