#pragma once
#include "io-device.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace rx::vm {
static constexpr std::uint64_t kPageShift = 14;
static constexpr std::uint64_t kPageSize = static_cast<std::uint64_t>(1)
                                           << kPageShift;

enum BlockFlags {
  kBlockFlagFlexibleMemory = 1 << 0,
  kBlockFlagDirectMemory = 1 << 1,
  kBlockFlagStack = 1 << 2,
  kBlockFlagPooledMemory = 1 << 3,
  kBlockFlagCommited = 1 << 4,
};

enum MapFlags {
  kMapFlagShared = 0x1,
  kMapFlagPrivate = 0x2,
  kMapFlagFixed = 0x10,
  kMapFlagRename = 0x20,
  kMapFlagNoReserve = 0x40,
  kMapFlagNoOverwrite = 0x80,
  kMapFlagVoid = 0x100,
  kMapFlagHasSemaphore = 0x200,
  kMapFlagStack = 0x400,
  kMapFlagNoSync = 0x800,
  kMapFlagAnonymous = 0x1000,
  kMapFlagSystem = 0x2000,
  kMapFlagAllAvailable = 0x4000,
  kMapFlagNoCore = 0x20000,
  kMapFlagPrefaultRead = 0x40000,
  kMapFlagSelf = 0x80000,
};

enum MapProt {
  kMapProtCpuRead = 1,
  kMapProtCpuWrite = 2,
  kMapProtCpuExec = 4,
  kMapProtCpuAll = 0x7,
  kMapProtGpuRead = 0x10,
  kMapProtGpuWrite = 0x20,
  kMapProtGpuAll = 0x30,
};

enum MapInternalFlags {
  kMapInternalReserveOnly = 1 << 0,
};

struct VirtualQueryInfo {
  std::uint64_t start;
  std::uint64_t end;
  std::uint64_t offset;
  std::int32_t protection;
  std::int32_t memoryType;
  std::uint32_t flags;
  char name[32];
};

static constexpr std::uint32_t kMapFlagsAlignShift = 24;
static constexpr std::uint32_t kMapFlagsAlignMask = 0x1f << kMapFlagsAlignShift;

std::string mapFlagsToString(std::int32_t flags);
std::string mapProtToString(std::int32_t prot);

void printHostStats();
void fork(std::uint64_t pid);
void reset();
void initialize();
void deinitialize();
void *map(void *addr, std::uint64_t len, std::int32_t prot, std::int32_t flags,
          std::int32_t internalFlags = 0, IoDevice *device = nullptr,
          std::uint64_t offset = 0);
bool unmap(void *addr, std::uint64_t size);
bool protect(void *addr, std::uint64_t size, std::int32_t prot);

void setName(std::uint64_t start, std::uint64_t size, const char *name);
bool virtualQuery(const void *addr, std::int32_t flags, VirtualQueryInfo *info);
bool queryProtection(const void *addr, std::uint64_t *startAddress,
                     std::uint64_t *endAddress, std::int32_t *prot);
unsigned getPageProtection(std::uint64_t address);
} // namespace rx::vm
