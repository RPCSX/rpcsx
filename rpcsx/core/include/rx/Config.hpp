#pragma once

namespace rx {
// FIXME: serialization
struct Config {
  int gpuIndex = 0;
  bool validateGpu = false;
  bool disableGpuCache = false;
  bool debugGpu = false;
};

extern Config g_config;
} // namespace rx
