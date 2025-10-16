#include "mem.hpp"
#include "die.hpp"

#ifdef _WIN32
#define NTDDI_VERSION NTDDI_WIN10_NI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef _WIN32
const std::size_t rx::mem::pageSize = [] {
  SYSTEM_INFO info;
  ::GetSystemInfo(&info);
  return info.dwPageSize;
}();
#else
const std::size_t rx::mem::pageSize = sysconf(_SC_PAGE_SIZE);
#endif

std::errc rx::mem::reserve(rx::AddressRange range) {
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

std::errc rx::mem::release(rx::AddressRange range,
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

std::errc rx::mem::protect(rx::AddressRange range,
                           rx::EnumBitSet<Protection> prot) {
  auto pointer = std::bit_cast<void *>(range.beginAddress());
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

  auto rawProt =
      (prot & (mem::Protection::R | mem::Protection::W | mem::Protection::X))
          .toUnderlying();
  auto wProt = protTable[rawProt];

  if (!VirtualProtect(pointer, range.size(), wProt, nullptr)) {
    return std::errc::invalid_argument;
  }
#else
  if (::mprotect(pointer, range.size(), prot.toUnderlying())) {
    return std::errc{errno};
  }
#endif

  return {};
}

std::vector<rx::mem::VirtualQueryEntry> rx::mem::query(rx::AddressRange range) {
  std::vector<VirtualQueryEntry> result;

#ifdef _WIN32
  std::uintptr_t address = range.beginAddress();
  while (address < range.endAddress()) {
    MEMORY_BASIC_INFORMATION info;
    if (!VirtualQuery((void *)address, &info, sizeof(info))) {
      rx::die("VirtualQuery: failed, address = {:x}", address);
    }

    auto region = rx::AddressRange::fromBeginSize(
                      (std::uintptr_t)info.BaseAddress, info.RegionSize)
                      .intersection(range);

    address = region.endAddress();

    if (info.State == MEM_FREE || !region.isValid() ||
        !range.contains(region)) {
      continue;
    }

    rx::EnumBitSet<Protection> flags = {};

    switch (info.AllocationProtect & 0xff) {
    case PAGE_NOACCESS:
      break;
    case PAGE_READONLY:
      flags = Protection::R;
      break;
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
      flags = Protection::R | Protection::W;
      break;
    case PAGE_EXECUTE:
      flags = Protection::X;
      break;
    case PAGE_EXECUTE_READ:
      flags = Protection::X | Protection::R;
      break;

    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
      flags = Protection::X | Protection::W | Protection::R;
      break;
    }

    result.emplace_back(region, flags);
  }
#elif defined(__linux)
  char buf[1024];
  auto maps = std::fopen("/proc/self/maps", "r");

  while (std::fgets(buf, sizeof(buf), maps)) {
    std::uint64_t beginAddress;
    std::uint64_t endAddress;
    char flagChars[5];

    std::sscanf(buf, "%lx-%lx %4s", &beginAddress, &endAddress, flagChars);

    auto region = rx::AddressRange::fromBeginEnd(beginAddress, endAddress)
                      .intersection(range);

    if (!region.isValid()) {
      continue;
    }

    if (region.beginAddress() >= range.beginAddress()) {
      break;
    }

    rx::EnumBitSet<Protection> flags = {};

    if (flagChars[0] == 'r') {
      flags |= Protection::R;
    }

    if (flagChars[1] == 'w') {
      flags |= Protection::W;
    }

    if (flagChars[2] == 'x') {
      flags |= Protection::X;
    }

    result.emplace_back(region, flags);
  }

  std::fclose(maps);
#elif defined(__APPLE__)
  // FIXME: use mach_vm_region_info?
  // workaround: assume all pages are not used
#else
#error "Not implemented"
#endif
  return result;
}
