#pragma once

#include "ModuleHandle.hpp"
#include "ModuleSegment.hpp"

#include "../KernelAllocator.hpp"
#include "../utils/Rc.hpp"

#include "orbis-config.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace orbis {
struct Thread;
struct Process;

enum class DynType : std::uint8_t {
  None,
  FreeBsd,
  Ps4,
  Ps5,
};

struct ModuleNeeded {
  utils::kstring name;
  std::uint16_t version;
  std::uint64_t attr;
  bool isExport;
};

enum class SymbolBind : std::uint8_t { Local, Global, Weak, Unique = 10 };

enum class SymbolVisibility : std::uint8_t {
  Default,
  Internal,
  Hidden,
  Protected
};

enum class SymbolType : std::uint8_t {
  NoType,
  Object,
  Function,
  Section,
  File,
  Common,
  Tls,
  IFunc = 10,
};

struct Symbol {
  std::int32_t moduleIndex;
  std::uint32_t libraryIndex;
  std::uint64_t id;
  std::uint64_t address;
  std::uint64_t size;
  SymbolVisibility visibility;
  SymbolBind bind;
  SymbolType type;
};

struct Relocation {
  std::uint64_t offset;
  std::uint32_t relType;
  std::uint32_t symbolIndex;
  std::int64_t addend;
};

struct Module final {
  Process *proc{};
  utils::kstring vfsPath;
  char moduleName[256]{};
  char soName[256]{};
  ModuleHandle id{};
  uint32_t tlsIndex{};
  ptr<void> tlsInit{};
  uint32_t tlsInitSize{};
  uint32_t tlsSize{};
  uint32_t tlsOffset{};
  uint32_t tlsAlign{};
  ptr<void> initProc{};
  ptr<void> finiProc{};
  ptr<void> ehFrameHdr{};
  ptr<void> ehFrame{};
  uint32_t ehFrameHdrSize{};
  uint32_t ehFrameSize{};
  ModuleSegment segments[16]{};
  uint32_t segmentCount{};
  std::uint8_t fingerprint[20]{};
  ptr<void> base{};
  uint64_t size{};
  ptr<void> stackStart{};
  ptr<void> stackEnd{};
  ptr<void> processParam{};
  uint64_t processParamSize{};
  ptr<void> moduleParam{};
  uint64_t moduleParamSize{};

  ptr<uint64_t> pltGot{};

  uint64_t attributes{};
  uint16_t version{};
  uint16_t type{};
  uint16_t flags{};
  uint64_t entryPoint{};

  DynType dynType = DynType::None;

  uint32_t phNum{};
  uint64_t phdrAddress{};

  bool isTlsDone = false;

  utils::kstring interp;
  utils::kvector<Symbol> symbols;
  utils::kvector<Relocation> pltRelocations;
  utils::kvector<Relocation> nonPltRelocations;
  utils::kvector<ModuleNeeded> neededModules;
  utils::kvector<ModuleNeeded> neededLibraries;
  utils::kvector<utils::Ref<Module>> importedModules;
  utils::kvector<utils::Ref<Module>> namespaceModules;
  utils::kvector<utils::kstring> needed;

  std::atomic<unsigned> references{0};
  unsigned _total_size = 0;

  void incRef() {
    if (_total_size != sizeof(Module))
      std::abort();
    if (references.fetch_add(1, std::memory_order::relaxed) > 512) {
      assert(!"too many references");
    }
  }

  void decRef() {
    if (references.fetch_sub(1, std::memory_order::relaxed) == 1 &&
        proc != nullptr) {
      destroy();
    }
  }

  orbis::SysResult relocate(Process *process);

private:
  void destroy();
};

utils::Ref<Module> createModule(Thread *p, std::string vfsPath,
                                const char *name);
} // namespace orbis
