#pragma once

#include <string>

namespace rx {
// Get the full path to the current executable
std::string getExecutablePath();

// Find an executable by name in system PATH
// Returns the full path if found, empty string otherwise
std::string findExecutable(const std::string &name);
} // namespace rx
