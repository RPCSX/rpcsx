#include "bridge.hpp"

#include <fcntl.h>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

static int gShmFd = -1;
static constexpr std::size_t kShmSize = sizeof(amdgpu::bridge::BridgeHeader) +
                                        (sizeof(std::uint64_t) * (1024 * 1024));
std::uint32_t amdgpu::bridge::expGpuPid = 0;

amdgpu::bridge::BridgeHeader *
amdgpu::bridge::createShmCommandBuffer(const char *name) {
  if (gShmFd != -1) {
    return nullptr;
  }

  // unlinkShm(name);

  int fd = ::shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    return nullptr;
  }

  if (ftruncate(fd, kShmSize) < 0) {
    ::close(fd);
    return nullptr;
  }

  void *memory =
      ::mmap(nullptr, kShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (memory == MAP_FAILED) {
    ::close(fd);
    return nullptr;
  }

  gShmFd = fd;
  auto result = new (memory) amdgpu::bridge::BridgeHeader();
  result->size =
      (kShmSize - sizeof(amdgpu::bridge::BridgeHeader)) / sizeof(std::uint64_t);
  return result;
}

amdgpu::bridge::BridgeHeader *
amdgpu::bridge::openShmCommandBuffer(const char *name) {
  if (gShmFd != -1) {
    return nullptr;
  }

  int fd = ::shm_open(name, O_RDWR, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    return nullptr;
  }

  if (ftruncate(fd, kShmSize) < 0) {
    ::close(fd);
    return nullptr;
  }

  void *memory =
      ::mmap(nullptr, kShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (memory == MAP_FAILED) {
    ::close(fd);
    return nullptr;
  }

  gShmFd = fd;
  return new (memory) amdgpu::bridge::BridgeHeader;
}

void amdgpu::bridge::destroyShmCommandBuffer(
    amdgpu::bridge::BridgeHeader *buffer) {
  if (gShmFd == -1) {
    __builtin_trap();
  }

  buffer->~BridgeHeader();
  ::close(gShmFd);
  gShmFd = -1;
  ::munmap(buffer, kShmSize);
}

void amdgpu::bridge::unlinkShm(const char *name) { ::shm_unlink(name); }
