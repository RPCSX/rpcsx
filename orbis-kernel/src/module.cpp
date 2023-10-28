#include "module/Module.hpp"
#include "KernelAllocator.hpp"
#include "thread.hpp"
#include <utility>

#include "thread/Process.hpp"
#include <string_view>

// TODO: move relocations to the platform specific code
enum RelType {
  kRelNone,
  kRel64,
  kRelPc32,
  kRelGot32,
  kRelPlt32,
  kRelCopy,
  kRelGlobDat,
  kRelJumpSlot,
  kRelRelative,
  kRelGotPcRel,
  kRel32,
  kRel32s,
  kRel16,
  kRelPc16,
  kRel8,
  kRelPc8,
  kRelDtpMod64,
  kRelDtpOff64,
  kRelTpOff64,
  kRelTlsGd,
  kRelTlsLd,
  kRelDtpOff32,
  kRelGotTpOff,
  kRelTpOff32,
  kRelPc64,
  kRelGotOff64,
  kRelGotPc32,
  kRelGot64,
  kRelGotPcRel64,
  kRelGotPc64,
  kRelGotPlt64,
  kRelPltOff64,
  kRelSize32,
  kRelSize64,
  kRelGotPc32TlsDesc,
  kRelTlsDescCall,
  kRelTlsDesc,
  kRelIRelative,
  kRelRelative64,
};

static std::uint64_t calculateTlsOffset(std::uint64_t prevOffset,
                                        std::uint64_t size,
                                        std::uint64_t align) {
  return (prevOffset + size + align - 1) & ~(align - 1);
}

static void allocateTlsOffset(orbis::Process *process, orbis::Module *module) {
  if (module->isTlsDone) {
    return;
  }

  auto offset =
      calculateTlsOffset(module->tlsIndex == 1 ? 0 : process->lastTlsOffset,
                         module->tlsSize, module->tlsAlign);

  module->tlsOffset = offset;
  process->lastTlsOffset = offset;
  module->isTlsDone = true;
}

static orbis::SysResult doPltRelocation(orbis::Process *process,
                                        orbis::Module *module,
                                        orbis::Relocation rel) {
  auto symbol = module->symbols.at(rel.symbolIndex);

  auto A = rel.addend;
  auto B = reinterpret_cast<std::uint64_t>(module->base);
  auto where = reinterpret_cast<std::uint64_t *>(B + rel.offset);
  auto where32 = reinterpret_cast<std::uint32_t *>(B + rel.offset);
  auto P = reinterpret_cast<std::uintptr_t>(where);

  auto findDefModule = [module,
                        symbol] -> std::pair<orbis::Module *, std::uint64_t> {
    if (symbol.moduleIndex == -1 || symbol.bind == orbis::SymbolBind::Local) {
      return std::pair(module, symbol.address);
    }

    auto &defModule = module->importedModules.at(symbol.moduleIndex);
    if (!defModule) {
      // std::printf(
      //     "Delaying plt relocation '%s' ('%s'), symbol '%llx' in %s
      //     module\n", module->moduleName, module->soName, (unsigned long
      //     long)symbol.id,
      //     module->neededModules[symbol.moduleIndex].name.c_str());

      return {};
    }

    auto library = module->neededLibraries.at(symbol.libraryIndex);

    std::vector<std::string> foundInLibs;
    for (auto defSym : defModule->symbols) {
      if (defSym.id != symbol.id || defSym.bind == orbis::SymbolBind::Local) {
        continue;
      }

      if (defSym.visibility == orbis::SymbolVisibility::Hidden) {
        std::printf("Ignoring hidden symbol\n");
        continue;
      }

      auto defLib = defModule->neededLibraries.at(defSym.libraryIndex);

      if (defLib.name == library.name) {
        return std::pair(defModule.get(), defSym.address);
      }

      foundInLibs.emplace_back(std::string_view(defLib.name));
    }

    for (auto nsDefModule : defModule->namespaceModules) {
      for (auto defSym : nsDefModule->symbols) {
        if (defSym.id != symbol.id || defSym.bind == orbis::SymbolBind::Local) {
          continue;
        }

        if (defSym.visibility == orbis::SymbolVisibility::Hidden) {
          std::printf("Ignoring hidden symbol\n");
          continue;
        }

        auto defLib = nsDefModule->neededLibraries.at(defSym.libraryIndex);

        if (defLib.name == library.name) {
          return std::pair(nsDefModule.get(), defSym.address);
        }
      }
    }

    std::printf(
        "'%s' ('%s') uses undefined symbol '%llx' in '%s' ('%s') module\n",
        module->moduleName, module->soName, (unsigned long long)symbol.id,
        defModule->moduleName, defModule->soName);
    if (foundInLibs.size() > 0) {
      std::printf("Requested library is '%s', exists in libraries: [",
                  library.name.c_str());

      for (bool isFirst = true; auto &lib : foundInLibs) {
        if (isFirst) {
          isFirst = false;
        } else {
          std::printf(", ");
        }

        std::printf("'%s'", lib.c_str());
      }
      std::printf("]\n");
    }
    return std::pair(module, symbol.address);
  };

  switch (rel.relType) {
  case kRelJumpSlot: {
    bool isLazyBind = false; // TODO
    if (isLazyBind) {
      *where += B;
    } else {
      auto [defObj, S] = findDefModule();

      if (defObj == nullptr) {
        return orbis::ErrorCode::INVAL;
      }

      *where = reinterpret_cast<std::uintptr_t>(defObj->base) + S;
    }
    return {};
  }
  }

  std::fprintf(stderr, "unimplemented relocation type %u\n",
               (unsigned)rel.relType);
  std::abort();
  return {};
}

static orbis::SysResult doRelocation(orbis::Process *process,
                                     orbis::Module *module,
                                     orbis::Relocation rel) {
  auto symbol = module->symbols.at(rel.symbolIndex);

  auto A = rel.addend;
  auto B = reinterpret_cast<std::uint64_t>(module->base);
  auto where = reinterpret_cast<std::uint64_t *>(B + rel.offset);
  auto where32 = reinterpret_cast<std::uint32_t *>(B + rel.offset);
  auto P = reinterpret_cast<std::uintptr_t>(where);

  auto findDefModule = [module, symbol,
                        rel] -> std::pair<orbis::Module *, std::uint64_t> {
    if (symbol.moduleIndex == -1 || symbol.bind == orbis::SymbolBind::Local) {
      return std::pair(module, symbol.address);
    }

    auto &defModule = module->importedModules.at(symbol.moduleIndex);
    if (!defModule) {
      std::printf("'%s' ('%s') uses undefined symbol '%llx' in unloaded module "
                  "'%s', rel %u\n",
                  module->moduleName, module->soName,
                  (unsigned long long)symbol.id,
                  module->neededModules.at(symbol.moduleIndex).name.c_str(),
                  rel.relType);

      return {};
    }

    auto library = module->neededLibraries.at(symbol.libraryIndex);

    std::vector<std::string> foundInLibs;
    for (auto defSym : defModule->symbols) {
      if (defSym.id != symbol.id || defSym.bind == orbis::SymbolBind::Local) {
        continue;
      }

      if (defSym.visibility == orbis::SymbolVisibility::Hidden) {
        std::printf("Ignoring hidden symbol\n");
        continue;
      }

      auto defLib = defModule->neededLibraries.at(defSym.libraryIndex);

      if (defLib.name == library.name) {
        return std::pair(defModule.get(), defSym.address);
      }

      foundInLibs.emplace_back(std::string_view(defLib.name));
    }

    for (auto nsDefModule : defModule->namespaceModules) {
      for (auto defSym : nsDefModule->symbols) {
        if (defSym.id != symbol.id || defSym.bind == orbis::SymbolBind::Local) {
          continue;
        }

        if (defSym.visibility == orbis::SymbolVisibility::Hidden) {
          std::printf("Ignoring hidden symbol\n");
          continue;
        }

        auto defLib = nsDefModule->neededLibraries.at(defSym.libraryIndex);

        if (defLib.name == library.name) {
          return std::pair(nsDefModule.get(), defSym.address);
        }
      }
    }

    std::printf(
        "'%s' ('%s') uses undefined symbol '%llx' in '%s' ('%s') module\n",
        module->moduleName, module->soName, (unsigned long long)symbol.id,
        defModule->moduleName, defModule->soName);
    if (foundInLibs.size() > 0) {
      std::printf("Requested library is '%s', exists in libraries: [",
                  library.name.c_str());

      for (bool isFirst = true; auto &lib : foundInLibs) {
        if (isFirst) {
          isFirst = false;
        } else {
          std::printf(", ");
        }

        std::printf("'%s'", lib.c_str());
      }
      std::printf("]\n");
    }
    return std::pair(module, symbol.address);
  };

  switch (rel.relType) {
  case kRelNone:
    return {};
  case kRel64: {
    auto [defObj, S] = findDefModule();

    if (defObj == nullptr) {
      return orbis::ErrorCode::INVAL;
    }
    *where = reinterpret_cast<std::uintptr_t>(defObj->base) + S + A;
    return {};
  }
    return {};
  case kRelPc32: {
    auto [defObj, S] = findDefModule();

    if (defObj == nullptr) {
      return orbis::ErrorCode::INVAL;
    }
    *where32 = reinterpret_cast<std::uintptr_t>(defObj->base) + S + A - P;
    return {};
  }
  // case kRelCopy:
  //   return{};
  case kRelGlobDat: {
    auto [defObj, S] = findDefModule();

    if (defObj == nullptr) {
      return orbis::ErrorCode::INVAL;
    }
    *where = reinterpret_cast<std::uintptr_t>(defObj->base) + S;
    return {};
  }
  case kRelRelative:
    *where = B + A;
    return {};
  case kRelDtpMod64: {
    auto [defObj, S] = findDefModule();
    if (defObj == nullptr) {
      return orbis::ErrorCode::INVAL;
    }
    *where += defObj->tlsIndex;
    return {};
  }
  case kRelDtpOff64: {
    auto [defObj, S] = findDefModule();
    if (defObj == nullptr) {
      return orbis::ErrorCode::INVAL;
    }
    *where += S + A;
    return {};
  }
  case kRelTpOff64: {
    auto [defObj, S] = findDefModule();
    if (defObj == nullptr) {
      return orbis::ErrorCode::INVAL;
    }
    if (!defObj->isTlsDone) {
      allocateTlsOffset(process, defObj);
    }
    *where = S - defObj->tlsOffset + A;
    return {};
  }
  case kRelDtpOff32: {
    auto [defObj, S] = findDefModule();
    *where32 += S + A;
    return {};
  }
  case kRelTpOff32: {
    auto [defObj, S] = findDefModule();
    if (defObj == nullptr) {
      return orbis::ErrorCode::INVAL;
    }
    if (!defObj->isTlsDone) {
      allocateTlsOffset(process, defObj);
    }
    *where32 = S - defObj->tlsOffset + A;
    return {};
  }
  }

  std::fprintf(stderr, "unimplemented relocation type %u\n",
               (unsigned)rel.relType);
  std::abort();
  return {};
}

orbis::SysResult orbis::Module::relocate(Process *process) {
  if (!pltRelocations.empty()) {
    kvector<Relocation> delayedRelocations;
    std::size_t resolved = 0;
    std::size_t delayed = 0;
    for (auto rel : pltRelocations) {
      auto result = doPltRelocation(process, this, rel);

      if (result.isError()) {
        delayedRelocations.push_back(rel);
        ++delayed;
      } else {
        ++resolved;
      }
    }

    std::printf("plt relocation of %s: delayed/resolved: %zu/%zu\n", moduleName,
                delayed, resolved);
    pltRelocations = std::move(delayedRelocations);
  }

  if (!nonPltRelocations.empty()) {
    kvector<Relocation> delayedRelocations;
    std::size_t resolved = 0;
    std::size_t delayed = 0;
    for (auto rel : nonPltRelocations) {
      auto result = doRelocation(process, this, rel);

      if (result.isError()) {
        delayedRelocations.push_back(rel);
        ++delayed;
      } else {
        ++resolved;
      }
    }

    std::printf("non-plt relocation of %s: delayed/resolved: %zu/%zu\n",
                moduleName, delayed, resolved);
    nonPltRelocations = std::move(delayedRelocations);
  }

  return {};
}

void orbis::Module::destroy() {
  std::lock_guard lock(proc->mtx);
  proc->modulesMap.close(id);
}
