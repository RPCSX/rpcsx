#pragma once

#include <filesystem>
#include <string_view>

namespace rx {
const char *getShmPath();
std::filesystem::path getShmGuestPath(std::string_view path);
void createGpuDevice();
void shutdown();
void attachProcess(int pid);
void startWatchdog();
} // namespace rx
