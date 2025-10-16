#include "Mappable.hpp"
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

std::errc rx::reserveVirtualSpace(rx::AddressRange range) {
  auto pointer = std::bit_cast<void *>(range.beginAddress());

#ifdef _WIN32
  auto reservation = VirtualAlloc2(nullptr, pointer, range.size(),
                                   MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                                   PAGE_NOACCESS, nullptr, 0);

  if (reservation == nullptr) {
    return std::errc::invalid_argument;
  }
#else
#ifdef MAP_FIXED_NOREPLACE
  static constexpr auto kMapFixedNoReplace = MAP_FIXED_NOREPLACE;
#else
  static constexpr auto kMapFixedNoReplace = MAP_FIXED;
#endif

  auto reservation = ::mmap(pointer, range.size(), PROT_NONE,
                            MAP_ANON | kMapFixedNoReplace | MAP_PRIVATE, -1, 0);

  if (reservation == MAP_FAILED) {
    return std::errc{errno};
  }
#endif
  return {};
}

std::errc rx::releaseVirtualSpace(rx::AddressRange range,
                                  [[maybe_unused]] std::size_t alignment) {
#ifdef _WIN32
  // simple and stupid implementation
  for (std::uintptr_t address = range.beginAddress();
       address < range.endAddress(); address += alignment) {
    auto pointer = std::bit_cast<void *>(address);
    if (!UnmapViewOfFileEx(pointer, MEM_PRESERVE_PLACEHOLDER)) {
      VirtualFree(pointer, alignment, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    }
  }
#else
  auto pointer = std::bit_cast<void *>(range.beginAddress());

  auto reservation = ::mmap(pointer, range.size(), PROT_NONE,
                            MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);

  if (!reservation || reservation != pointer) {
    return std::errc{errno};
  }
#endif

  return {};
}

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
  auto fd = ::memfd_create("", 0);
  if (fd < 0) {
    return {{}, std::errc{errno}};
  }

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
                            rx::EnumBitSet<Protection> protection,
                            [[maybe_unused]] std::size_t alignment) {
#ifdef _WIN32
  DWORD prot = 0;
  if (!protection) {
    prot = PAGE_NOACCESS;
  } else if (protection == Protection::R) {
    prot = PAGE_READONLY;
  } else if (protection & Protection::X) {
    if (protection & Protection::W) {
      prot = PAGE_EXECUTE_READWRITE;
    } else if (protection & Protection::R) {
      prot = PAGE_EXECUTE_READ;
    } else {
      prot = PAGE_EXECUTE;
    }
  } else {
    prot = PAGE_READWRITE;
  }

  releaseVirtualSpace(virtualRange, alignment);

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

  return {};
#else
  int prot = 0;

  if (protection & Protection::R) {
    prot |= PROT_READ;
  }
  if (protection & Protection::W) {
    prot |= PROT_READ | PROT_WRITE;
  }
  if (protection & Protection::X) {
    prot |= PROT_EXEC;
  }

  auto address = std::bit_cast<void *>(virtualRange.beginAddress());

  auto result = ::mmap(address, virtualRange.size(), prot,
                       MAP_SHARED | MAP_FIXED, m_handle, offset);

  if (result == MAP_FAILED) {
    return std::errc{errno};
  }

  return {};
#endif
}

void rx::Mappable::destroy() {
#ifdef _WIN32
  CloseHandle((HANDLE)m_handle);
#else
  ::close(m_handle);
#endif
}
