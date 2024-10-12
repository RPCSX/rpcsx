#pragma once

#include <cstddef>

namespace rx::mem {
extern const std::size_t pageSize;
void *map(void *address, std::size_t size, int prot, int flags, int fd = -1,
          std::ptrdiff_t offset = 0);
void *reserve(std::size_t size);
bool reserve(void *address, std::size_t size);
bool protect(void *address, std::size_t size, int prot);
bool unmap(void *address, std::size_t size);
void printStats();
} // namespace rx::mem
