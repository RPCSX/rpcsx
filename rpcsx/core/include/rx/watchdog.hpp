#pragma once

#include <filesystem>
#include <string_view>

namespace rx {
const char *getShmPath();
std::filesystem::path getShmGuestPath(std::string_view path);
void createGpuDevice();
void shutdown();
int startWatchdog();
} // namespace rx
