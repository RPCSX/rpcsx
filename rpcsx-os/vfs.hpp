#pragma once

#include "orbis/error/SysResult.hpp"
#include "orbis/utils/Rc.hpp"
#include <filesystem>

struct IoDevice;
struct IoDeviceInstance;

namespace rx::vfs {
void initialize();
void deinitialize();
orbis::SysResult mount(const std::filesystem::path &guestPath, IoDevice *dev);
orbis::SysResult open(std::string_view path, int flags, int mode,
                      orbis::Ref<IoDeviceInstance> *instance);
} // namespace vfs
