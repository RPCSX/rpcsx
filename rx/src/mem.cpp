#include "mem.hpp"
#include <sys/mman.h>
#include <unistd.h>

extern const std::size_t rx::mem::pageSize = sysconf(_SC_PAGE_SIZE);

void *rx::mem::map(void *address, std::size_t size, int prot, int flags,
                   int fd, std::ptrdiff_t offset) {
  return ::mmap(address, size, prot, flags, fd, offset);
}

void *rx::mem::reserve(std::size_t size) {
  return map(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS);
}

bool rx::mem::reserve(void *address, std::size_t size) {
  return map(address, size, PROT_NONE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED) != MAP_FAILED;
}

bool rx::mem::protect(void *address, std::size_t size, int prot) {
  return ::mprotect(address, size, prot) == 0;
}

bool rx::mem::unmap(void *address, std::size_t size) {
  return ::munmap(address, size) == 0;
}
