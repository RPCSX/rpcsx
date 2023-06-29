#pragma once

#include "orbis/module/Module.hpp"
#include "orbis/utils/Rc.hpp"
#include <cstddef>
#include <filesystem>
#include <span>

namespace rx::linker {
inline constexpr char nidLookup[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

constexpr std::uint64_t decodeNid(std::string_view nid) {
  std::uint64_t result = 0;

  if (nid.size() > 11) {
    std::abort();
  }

  for (std::size_t i = 0; i < nid.size(); ++i) {
    auto it = std::strchr(nidLookup, nid[i]);

    if (it == nullptr) {
      std::abort();
    }

    auto value = static_cast<uint32_t>(it - nidLookup);

    if (i == 10) {
      result <<= 4;
      result |= (value >> 2);
      break;
    }

    result <<= 6;
    result |= value;
  }

  return result;
}

std::uint64_t encodeFid(std::string_view fid);

struct Symbol {
  orbis::Module *module;
  std::uint64_t address;
  std::uint64_t bindMode;
};

enum OrbisElfType_t {
  kElfTypeNone = 0,
  kElfTypeRel = 1,
  kElfTypeExec = 2,
  kElfTypeDyn = 3,
  kElfTypeCore = 4,
  kElfTypeNum = 5,
  kElfTypeSceExec = 0xfe00,
  kElfTypeSceDynExec = 0xfe10,
  kElfTypeSceDynamic = 0xfe18
};

void override(std::string originalModuleName, std::filesystem::path replacedModulePath);
orbis::Ref<orbis::Module> loadModule(std::span<std::byte> image, orbis::Process *process);
orbis::Ref<orbis::Module> loadModuleFile(const char *path, orbis::Process *process);
orbis::Ref<orbis::Module> loadModuleByName(std::string_view name, orbis::Process *process);
} // namespace re::loader
