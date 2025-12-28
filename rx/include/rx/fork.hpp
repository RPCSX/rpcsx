#pragma once

#include <rx/Process.hpp>

namespace rx {
// Fork result: -1 on error, 0 in child, child PID in parent
ProcessId fork();
} // namespace rx
