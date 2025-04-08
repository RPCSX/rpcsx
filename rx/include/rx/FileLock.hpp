#pragma once

#include <cstdio>
#include <utility>

namespace rx {
void fileLock(std::FILE *file);
void fileUnlock(std::FILE *file);

class ScopedFileLock {
  std::FILE *mFile = nullptr;

public:
  ScopedFileLock() = default;
  ScopedFileLock(const ScopedFileLock &) = delete;
  ScopedFileLock &operator=(const ScopedFileLock &) = delete;

  ScopedFileLock(ScopedFileLock &&other) noexcept
      : mFile(std::exchange(other.mFile, nullptr)) {}
  ScopedFileLock &operator=(ScopedFileLock &&other) noexcept {
    std::swap(mFile, other.mFile);
    return *this;
  }

  ScopedFileLock(std::FILE *file) : mFile(file) {
    if (file) {
      fileLock(file);
    }
  }

  ~ScopedFileLock() {
    if (mFile) {
      fileUnlock(mFile);
    }
  }
};
} // namespace rx
