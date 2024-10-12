#pragma once

namespace rx {
// FIXME: serialization
struct Config {
  int gpuIndex = 0;
  bool validateGpu = false;
};

extern Config g_config;
} // namespace rx
