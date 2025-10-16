#include "Mappable.hpp"
#include "mem.hpp"
#include <system_error>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <cstdint>
#define NTDDI_VERSION NTDDI_WIN10_NI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef ANDROID
#include "format-base.hpp"
#endif

std::pair<rx::Mappable, std::errc>
rx::Mappable::CreateMemory(std::size_t size) {
  rx::Mappable result;

#ifdef _WIN32
  auto handle = CreateFileMapping2(INVALID_HANDLE_VALUE, nullptr,
                                   FILE_MAP_ALL_ACCESS, PAGE_EXECUTE_READWRITE,
                                   SEC_COMMIT, size, nullptr, nullptr, 0);

  if (!handle) {
    return {rx::Mappable{}, std::errc::invalid_argument};
  }

  result.m_handle = handle;
#else
#ifdef ANDROID
  auto name = rx::format("/{}-{:x}", (void *)&result, size);
  auto fd = ::shm_open(name.c_str(), O_CREAT | O_TRUNC, 0666);
#else
  auto fd = ::memfd_create("", 0);
#endif

  if (fd < 0) {
    return {{}, std::errc{errno}};
  }

#ifdef ANDROID
  ::shm_unlink(name.c_str());
#endif

  result.m_handle = fd;

  if (::ftruncate(fd, size) < 0) {
    return {{}, std::errc{errno}};
  }
#endif

  return {std::move(result), std::errc{}};
}

std::pair<rx::Mappable, std::errc> rx::Mappable::CreateSwap(std::size_t size) {
#ifdef _WIN32
  return CreateMemory(size);
#else
  char temp_filename[] = "./.rx-swap-XXXXXXXXXXX";
  int fd = ::mkstemp(temp_filename);
  if (fd < 0) {
    return {{}, std::errc{errno}};
  }
  ::unlink(temp_filename);

  rx::Mappable result;
  result.m_handle = fd;

  if (::ftruncate(fd, size) < 0) {
    return {{}, std::errc{errno}};
  }

  return {std::move(result), {}};
#endif
}

std::errc rx::Mappable::map(rx::AddressRange virtualRange, std::size_t offset,
                            rx::EnumBitSet<mem::Protection> protection,
                            [[maybe_unused]] std::size_t alignment) {
#ifdef _WIN32
  static const DWORD protTable[] = {
      PAGE_NOACCESS,          // 0
      PAGE_READONLY,          // R
      PAGE_EXECUTE_READWRITE, // W
      PAGE_EXECUTE_READWRITE, // RW
      PAGE_EXECUTE,           // X
      PAGE_EXECUTE_READWRITE, // XR
      PAGE_EXECUTE_READWRITE, // XW
      PAGE_EXECUTE_READWRITE, // XRW
  };

  auto prot = protTable[(protection & (mem::Protection::R | mem::Protection::W |
                                       mem::Protection::X))
                            .toUnderlying()];

  mem::release(virtualRange, alignment);

  for (std::uintptr_t address = virtualRange.beginAddress();
       address < virtualRange.endAddress();
       address += alignment, offset += alignment) {
    auto pointer = std::bit_cast<void *>(address);

    auto result =
        MapViewOfFile3((HANDLE)m_handle, nullptr, pointer, offset, alignment,
                       MEM_REPLACE_PLACEHOLDER, prot, nullptr, 0);

    if (!result) {
      return std::errc::invalid_argument;
    }
  }
#else
  int prot = 0;

  if (protection & mem::Protection::R) {
    prot |= PROT_READ;
  }
  if (protection & mem::Protection::W) {
    prot |= PROT_READ | PROT_WRITE;
  }
  if (protection & mem::Protection::X) {
    prot |= PROT_EXEC;
  }

  auto address = std::bit_cast<void *>(virtualRange.beginAddress());

  auto result = ::mmap(address, virtualRange.size(), prot,
                       MAP_SHARED | MAP_FIXED, m_handle, offset);

  if (result == MAP_FAILED) {
    return std::errc{errno};
  }
#endif

  return {};
}

void rx::Mappable::destroy() {
#ifdef _WIN32
  CloseHandle((HANDLE)m_handle);
#else
  ::close(m_handle);
#endif
}
