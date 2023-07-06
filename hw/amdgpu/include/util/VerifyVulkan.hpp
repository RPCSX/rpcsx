#pragma once
#include "Verify.hpp"
#include <vulkan/vulkan_core.h>

inline Verify operator<<(Verify lhs, VkResult result) {
  if (result < VK_SUCCESS) {
    auto location = lhs.location();
    util::unreachable("Verification failed at %s: %s:%u:%u(res = %d)",
                      location.function_name(), location.file_name(),
                      location.line(), location.column(), result);
  }

  return lhs;
}
