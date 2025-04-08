#include "FileLock.hpp"
#include <cstdio>

void rx::fileLock(std::FILE *file) {
#ifdef _WIN32
  _lock_file(file);
#else
  flockfile(file);
#endif
}

void rx::fileUnlock(std::FILE *file) {
#ifdef _WIN32
  _unlock_file(file);
#else
  funlockfile(file);
#endif
}
