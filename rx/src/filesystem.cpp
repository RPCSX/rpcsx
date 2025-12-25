#include <rx/filesystem.hpp>
#include <rx/types.hpp>

#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include <climits>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__NetBSD__) | defined(__OpenBSD__)
#include <sys/sysctl.h>
#elif defined(__sun)
#include <cstdlib>
#endif

std::string rx::getExecutablePath() {
#ifdef _WIN32
  wchar_t path[MAX_PATH];
  DWORD len = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (len == 0 || len == MAX_PATH) {
    return {};
  }

  // Convert wide string to narrow string
  int size =
      ::WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    return {};
  }

  std::string result(size - 1, '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, path, -1, result.data(), size, nullptr,
                        nullptr);
  return result;

#elif defined(__APPLE__)
  char path[PATH_MAX];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    return path;
  }

  // Buffer too small, allocate larger
  std::string result(size, '\0');
  if (_NSGetExecutablePath(result.data(), &size) == 0) {
    result.resize(size - 1); // Remove null terminator
    return result;
  }
  return {};

#elif defined(__linux__) || defined(__ANDROID__)
  char path[PATH_MAX];
  ssize_t len = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len == -1) {
    return {};
  }
  path[len] = '\0';
  return path;

#elif defined(__FreeBSD__) || defined(__DragonFly__)
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
  char path[PATH_MAX];
  size_t len = sizeof(path);
  if (sysctl(mib, 4, path, &len, nullptr, 0) == 0) {
    return path;
  }
  return {};

#elif defined(__NetBSD__)
  int mib[4] = {CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME};
  char path[PATH_MAX];
  size_t len = sizeof(path);
  if (sysctl(mib, 4, path, &len, nullptr, 0) == 0) {
    return path;
  }
  return {};

#elif defined(__OpenBSD__)
  // OpenBSD doesn't provide a direct way to get executable path
  // Return empty string as fallback
  return {};

#elif defined(__sun)
  // Solaris
  const char *path = getexecname();
  if (path) {
    if (path[0] == '/') {
      return std::string(path);
    }
    // Relative path, need to prepend current directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
      return std::string(cwd) + "/" + path;
    }
  }
  return {};

#else
#error "rx::filesystem::getExecutablePath not implemented for this platform"
#endif
}

std::string rx::findExecutable(const std::string &name) {
  if (name.empty()) {
    return {};
  }

#ifdef _WIN32
  // On Windows, search PATH with automatic .exe extension handling
  std::vector<std::string> extensions;

  // Check if name already has an extension
  bool hasExtension = name.find('.') != std::string::npos;
  if (hasExtension) {
    extensions.push_back("");
  } else {
    // Try common executable extensions
    const char *pathExt = std::getenv("PATHEXT");
    if (pathExt) {
      std::string pathExtStr = pathExt;
      size_t start = 0;
      while (start < pathExtStr.size()) {
        size_t end = pathExtStr.find(';', start);
        if (end == std::string::npos) {
          end = pathExtStr.size();
        }
        std::string ext = pathExtStr.substr(start, end - start);
        extensions.push_back(ext);
        start = end + 1;
      }
    } else {
      extensions = {".exe", ".bat", ".cmd", ".com"};
    }
  }

  // Check if it's an absolute or relative path
  if (name.find('\\') != std::string::npos ||
      name.find('/') != std::string::npos ||
      (name.size() >= 2 && name[1] == ':')) {
    // It's a path, check if file exists
    for (const auto &ext : extensions) {
      std::string fullPath = name + ext;
      DWORD attrs = ::GetFileAttributesA(fullPath.c_str());
      if (attrs != INVALID_FILE_ATTRIBUTES &&
          !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return fullPath;
      }
    }
    return {};
  }

  // Search in PATH
  const char *pathEnv = std::getenv("PATH");
  if (!pathEnv) {
    return {};
  }

  std::string_view pathStr = pathEnv;
  size_t start = 0;
  while (start < pathStr.size()) {
    size_t end = pathStr.find(';', start);
    if (end == std::string::npos) {
      end = pathStr.size();
    }

    auto dir = std::string(pathStr.substr(start, end - start));
    if (!dir.empty()) {
      // Add separator if needed
      if (dir.back() != '\\' && dir.back() != '/') {
        dir += '\\';
      }

      for (const auto &ext : extensions) {
        std::string fullPath = dir + name + ext;
        DWORD attrs = ::GetFileAttributesA(fullPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES &&
            !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
          return fullPath;
        }
      }
    }

    start = end + 1;
  }

  return {};

#else
  // Unix-like systems
  // Check if it's already a path (contains /)
  if (name.find('/') != std::string::npos) {
    struct stat st;
    if (stat(name.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
        (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
      return name;
    }
    return {};
  }

  // Search in PATH
  const char *pathEnv = std::getenv("PATH");
  if (!pathEnv) {
    return {};
  }

  std::string pathStr = pathEnv;
  size_t start = 0;
  while (start < pathStr.size()) {
    size_t end = pathStr.find(':', start);
    if (end == std::string::npos) {
      end = pathStr.size();
    }

    std::string dir = pathStr.substr(start, end - start);
    if (!dir.empty()) {
      // Add separator if needed
      if (dir.back() != '/') {
        dir += '/';
      }

      std::string fullPath = dir + name;
      struct stat st;
      if (stat(fullPath.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
          (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        return fullPath;
      }
    }

    start = end + 1;
  }

  return {};
#endif
}
