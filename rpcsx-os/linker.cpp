#include "linker.hpp"
#include "align.hpp"
#include "io-device.hpp"
#include "orbis/module/Module.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include <crypto/sha1.h>
#include <elf.h>
#include <map>
#include <memory>
#include <orbis/thread/Process.hpp>
#include <sys/mman.h>
#include <unordered_map>

using orbis::utils::Ref;

std::uint64_t rx::linker::encodeFid(std::string_view fid) {
  static const char suffix[] =
      "\x51\x8D\x64\xA6\x35\xDE\xD8\xC1\xE6\xB0\x39\xB1\xC3\xE5\x52\x30";

  sha1_context ctx;
  unsigned char output[20];

  sha1_starts(&ctx);
  sha1_update(&ctx, reinterpret_cast<const unsigned char *>(fid.data()),
              fid.length());
  sha1_update(&ctx, reinterpret_cast<const unsigned char *>(suffix),
              sizeof(suffix) - 1);
  sha1_finish(&ctx, output);

  std::uint64_t hash;
  std::memcpy(&hash, output, sizeof(hash));
  return hash;
}

enum OrbisElfProgramType {
  kElfProgramTypeNull = 0,
  kElfProgramTypeLoad = 1,
  kElfProgramTypeDynamic = 2,
  kElfProgramTypeInterp = 3,
  kElfProgramTypeNote = 4,
  kElfProgramTypeShlib = 5,
  kElfProgramTypePhdr = 6,
  kElfProgramTypeTls = 7,
  kElfProgramTypeSceDynlibData = 0x61000000,
  kElfProgramTypeSceProcParam = 0x61000001,
  kElfProgramTypeSceModuleParam = 0x61000002,
  kElfProgramTypeSceRelRo = 0x61000010,
  kElfProgramTypeGnuEhFrame = 0x6474e550,
  kElfProgramTypeGnuRelRo = 0x6474e552,
  kElfProgramTypeSceComment = 0x6fffff00,
  kElfProgramTypeSceVersion = 0x6fffff01,
};

enum OrbisElfDynamicType {
  kElfDynamicTypeNull = 0,
  kElfDynamicTypeNeeded = 1,
  kElfDynamicTypePltRelSize = 2,
  kElfDynamicTypePltGot = 3,
  kElfDynamicTypeHash = 4,
  kElfDynamicTypeStrTab = 5,
  kElfDynamicTypeSymTab = 6,
  kElfDynamicTypeRela = 7,
  kElfDynamicTypeRelaSize = 8,
  kElfDynamicTypeRelaEnt = 9,
  kElfDynamicTypeStrSize = 10,
  kElfDynamicTypeSymEnt = 11,
  kElfDynamicTypeInit = 12,
  kElfDynamicTypeFini = 13,
  kElfDynamicTypeSoName = 14,
  kElfDynamicTypeRpath = 15,
  kElfDynamicTypeSymbolic = 16,
  kElfDynamicTypeRel = 17,
  kElfDynamicTypeRelSize = 18,
  kElfDynamicTypeRelEent = 19,
  kElfDynamicTypePltRel = 20,
  kElfDynamicTypeDebug = 21,
  kElfDynamicTypeTextRel = 22,
  kElfDynamicTypeJmpRel = 23,
  kElfDynamicTypeBindNow = 24,
  kElfDynamicTypeInitArray = 25,
  kElfDynamicTypeFiniArray = 26,
  kElfDynamicTypeInitArraySize = 27,
  kElfDynamicTypeFiniArraySize = 28,
  kElfDynamicTypeRunPath = 29,
  kElfDynamicTypeFlags = 30,
  kElfDynamicTypePreinitArray = 32,
  kElfDynamicTypePreinitArraySize = 33,
  kElfDynamicTypeSceFingerprint = 0x61000007,
  kElfDynamicTypeSceOriginalFilename = 0x61000009,
  kElfDynamicTypeSceModuleInfo = 0x6100000d,
  kElfDynamicTypeSceNeededModule = 0x6100000f,
  kElfDynamicTypeSceModuleAttr = 0x61000011,
  kElfDynamicTypeSceExportLib = 0x61000013,
  kElfDynamicTypeSceImportLib = 0x61000015,
  kElfDynamicTypeSceExportLibAttr = 0x61000017,
  kElfDynamicTypeSceImportLibAttr = 0x61000019,
  kElfDynamicTypeSceHash = 0x61000025,
  kElfDynamicTypeScePltGot = 0x61000027,
  kElfDynamicTypeSceJmpRel = 0x61000029,
  kElfDynamicTypeScePltRel = 0x6100002b,
  kElfDynamicTypeScePltRelSize = 0x6100002d,
  kElfDynamicTypeSceRela = 0x6100002f,
  kElfDynamicTypeSceRelaSize = 0x61000031,
  kElfDynamicTypeSceRelaEnt = 0x61000033,
  kElfDynamicTypeSceStrTab = 0x61000035,
  kElfDynamicTypeSceStrSize = 0x61000037,
  kElfDynamicTypeSceSymTab = 0x61000039,
  kElfDynamicTypeSceSymEnt = 0x6100003b,
  kElfDynamicTypeSceHashSize = 0x6100003d,
  kElfDynamicTypeSceOriginalFilename1 = 0x61000041,
  kElfDynamicTypeSceModuleInfo1 = 0x61000043,
  kElfDynamicTypeSceNeededModule1 = 0x61000045,
  kElfDynamicTypeSceImportLib1 = 0x61000049,
  kElfDynamicTypeSceSymTabSize = 0x6100003f,
  kElfDynamicTypeRelaCount = 0x6ffffff9
};

inline const char *toString(OrbisElfDynamicType dynType) {
  switch (dynType) {
  case kElfDynamicTypeNull:
    return "Null";
  case kElfDynamicTypeNeeded:
    return "Needed";
  case kElfDynamicTypePltRelSize:
    return "PltRelSize";
  case kElfDynamicTypePltGot:
    return "PltGot";
  case kElfDynamicTypeHash:
    return "Hash";
  case kElfDynamicTypeStrTab:
    return "StrTab";
  case kElfDynamicTypeSymTab:
    return "SymTab";
  case kElfDynamicTypeRela:
    return "Rela";
  case kElfDynamicTypeRelaSize:
    return "RelaSize";
  case kElfDynamicTypeRelaEnt:
    return "RelaEnt";
  case kElfDynamicTypeStrSize:
    return "StrSize";
  case kElfDynamicTypeSymEnt:
    return "SymEnt";
  case kElfDynamicTypeInit:
    return "Init";
  case kElfDynamicTypeFini:
    return "Fini";
  case kElfDynamicTypeSoName:
    return "SoName";
  case kElfDynamicTypeRpath:
    return "Rpath";
  case kElfDynamicTypeSymbolic:
    return "Symbolic";
  case kElfDynamicTypeRel:
    return "Rel";
  case kElfDynamicTypeRelSize:
    return "RelSize";
  case kElfDynamicTypeRelEent:
    return "RelEent";
  case kElfDynamicTypePltRel:
    return "PltRel";
  case kElfDynamicTypeDebug:
    return "Debug";
  case kElfDynamicTypeTextRel:
    return "TextRel";
  case kElfDynamicTypeJmpRel:
    return "JmpRel";
  case kElfDynamicTypeBindNow:
    return "BindNow";
  case kElfDynamicTypeInitArray:
    return "InitArray";
  case kElfDynamicTypeFiniArray:
    return "FiniArray";
  case kElfDynamicTypeInitArraySize:
    return "InitArraySize";
  case kElfDynamicTypeFiniArraySize:
    return "FiniArraySize";
  case kElfDynamicTypeRunPath:
    return "RunPath";
  case kElfDynamicTypeFlags:
    return "Flags";
  case kElfDynamicTypePreinitArray:
    return "PreinitArray";
  case kElfDynamicTypePreinitArraySize:
    return "PreinitArraySize";
  case kElfDynamicTypeSceFingerprint:
    return "SceFingerprint";
  case kElfDynamicTypeSceOriginalFilename:
    return "SceOriginalFilename";
  case kElfDynamicTypeSceModuleInfo:
    return "SceModuleInfo";
  case kElfDynamicTypeSceNeededModule:
    return "SceNeededModule";
  case kElfDynamicTypeSceModuleAttr:
    return "SceModuleAttr";
  case kElfDynamicTypeSceExportLib:
    return "SceExportLib";
  case kElfDynamicTypeSceImportLib:
    return "SceImportLib";
  case kElfDynamicTypeSceExportLibAttr:
    return "SceExportLibAttr";
  case kElfDynamicTypeSceImportLibAttr:
    return "SceImportLibAttr";
  case kElfDynamicTypeSceHash:
    return "SceHash";
  case kElfDynamicTypeScePltGot:
    return "ScePltGot";
  case kElfDynamicTypeSceJmpRel:
    return "SceJmpRel";
  case kElfDynamicTypeScePltRel:
    return "ScePltRel";
  case kElfDynamicTypeScePltRelSize:
    return "ScePltRelSize";
  case kElfDynamicTypeSceRela:
    return "SceRela";
  case kElfDynamicTypeSceRelaSize:
    return "SceRelaSize";
  case kElfDynamicTypeSceRelaEnt:
    return "SceRelaEnt";
  case kElfDynamicTypeSceStrTab:
    return "SceStrTab";
  case kElfDynamicTypeSceStrSize:
    return "SceStrSize";
  case kElfDynamicTypeSceSymTab:
    return "SceSymTab";
  case kElfDynamicTypeSceSymEnt:
    return "SceSymEnt";
  case kElfDynamicTypeSceHashSize:
    return "SceHashSize";
  case kElfDynamicTypeSceOriginalFilename1:
    return "SceOriginalFilename1";
  case kElfDynamicTypeSceModuleInfo1:
    return "SceModuleInfo1";
  case kElfDynamicTypeSceNeededModule1:
    return "SceNeededModule1";
  case kElfDynamicTypeSceImportLib1:
    return "SceImportLib1";
  case kElfDynamicTypeSceSymTabSize:
    return "SceSymTabSize";
  case kElfDynamicTypeRelaCount:
    return "RelaCount";
  }

  return "<unknown>";
}

struct SceProcessParam {
  std::uint64_t size = 0x40;
  std::uint32_t magic = 0x4942524F;
  std::uint32_t entryCount = 3;
  std::uint64_t sdkVersion = 0x4508101;

  std::uint64_t unk0 = 0;
  std::uint64_t unk1 = 0;
  std::uint64_t unk2 = 0;
  std::uint64_t unk3 = 0;

  std::uint64_t sceLibcParam_ptr = 0;

  // ext, size == 0x50
  std::uint64_t sceLibcKernelMemParam_ptr = 0;
  std::uint64_t sceLibcKernelFsParam_ptr = 0;
};

struct Symbol {
  orbis::Module *module;
  std::uint64_t address;
  std::uint64_t bindMode;
};

static std::map<std::string, std::filesystem::path, std::less<>>
    g_moduleOverrideTable;

void rx::linker::override(std::string originalModuleName,
                          std::filesystem::path replacedModulePath) {
  g_moduleOverrideTable[std::move(originalModuleName)] =
      std::move(replacedModulePath);
}

Ref<orbis::Module> rx::linker::loadModule(std::span<std::byte> image,
                                          orbis::Process *process) {
  Ref<orbis::Module> result{new orbis::Module{}};

  Elf64_Ehdr header;
  std::memcpy(&header, image.data(), sizeof(Elf64_Ehdr));
  result->type = header.e_type;

  Elf64_Phdr phdrsStorage[16];
  if (header.e_phnum > std::size(phdrsStorage)) {
    std::abort();
  }

  std::memcpy(phdrsStorage, image.data() + header.e_phoff,
              header.e_phnum * sizeof(Elf64_Phdr));
  auto phdrs = std::span(phdrsStorage, header.e_phnum);

  std::uint64_t imageSize = 0;
  std::uint64_t baseAddress = ~static_cast<std::uint64_t>(0);
  int dynamicPhdrIndex = -1;
  int interpPhdrIndex = -1;
  int notePhdrIndex = -1;
  int shlibPhdrIndex = -1;
  int phdrPhdrIndex = -1;
  int tlsPhdrIndex = -1;
  int sceDynlibDataPhdrIndex = -1;
  int sceProcParamIndex = -1;
  int sceModuleParamIndex = -1;
  int sceRelRoPhdrIndex = -1;
  int gnuEhFramePhdrIndex = -1;
  int gnuRelRoPhdrIndex = -1;
  int sceCommentPhdrIndex = -1;
  int sceVersionPhdrIndex = -1;

  for (auto &phdr : phdrs) {
    std::size_t index = &phdr - phdrs.data();

    switch (phdr.p_type) {
    case kElfProgramTypeNull:
      break;
    case kElfProgramTypeLoad:
      baseAddress =
          std::min(baseAddress, utils::alignDown(phdr.p_vaddr, phdr.p_align));
      imageSize = std::max(
          imageSize, utils::alignUp(phdr.p_vaddr + phdr.p_memsz, phdr.p_align));
      break;
    case kElfProgramTypeDynamic:
      dynamicPhdrIndex = index;
      break;
    case kElfProgramTypeInterp:
      interpPhdrIndex = index;
      break;
    case kElfProgramTypeNote:
      notePhdrIndex = index;
      break;
    case kElfProgramTypeShlib:
      shlibPhdrIndex = index;
      break;
    case kElfProgramTypePhdr:
      phdrPhdrIndex = index;
      break;
    case kElfProgramTypeTls:
      tlsPhdrIndex = index;
      break;
    case kElfProgramTypeSceDynlibData:
      sceDynlibDataPhdrIndex = index;
      break;
    case kElfProgramTypeSceProcParam:
      sceProcParamIndex = index;
      break;
    case kElfProgramTypeSceModuleParam:
      sceModuleParamIndex = index;
      break;
    case kElfProgramTypeSceRelRo:
      sceRelRoPhdrIndex = index;
      baseAddress =
          std::min(baseAddress, utils::alignDown(phdr.p_vaddr, phdr.p_align));
      imageSize = std::max(
          imageSize, utils::alignUp(phdr.p_vaddr + phdr.p_memsz, phdr.p_align));
      break;
    case kElfProgramTypeGnuEhFrame:
      gnuEhFramePhdrIndex = index;
      break;
    case kElfProgramTypeGnuRelRo:
      gnuRelRoPhdrIndex = index;
      break;
    case kElfProgramTypeSceComment:
      sceCommentPhdrIndex = index;
      break;
    case kElfProgramTypeSceVersion:
      sceVersionPhdrIndex = index;
      break;
    }
  }

  auto imageBase = reinterpret_cast<std::byte *>(
      rx::vm::map(reinterpret_cast<void *>(baseAddress),
                  utils::alignUp(imageSize, rx::vm::kPageSize),
                  0,
                  rx::vm::kMapFlagPrivate | rx::vm::kMapFlagAnonymous));

  if (imageBase == MAP_FAILED) {
    std::abort();
  }

  result->entryPoint =
      header.e_entry
          ? reinterpret_cast<std::uintptr_t>(imageBase + header.e_entry)
          : 0;

  if (sceProcParamIndex >= 0) {
    result->processParam =
        phdrs[sceProcParamIndex].p_vaddr
            ? reinterpret_cast<void *>(imageBase +
                                       phdrs[sceProcParamIndex].p_vaddr)
            : nullptr;
    result->processParamSize = phdrs[sceProcParamIndex].p_memsz;
  }

  if (sceModuleParamIndex >= 0) {
    result->moduleParam =
        phdrs[sceProcParamIndex].p_vaddr
            ? reinterpret_cast<void *>(imageBase +
                                       phdrs[sceModuleParamIndex].p_vaddr)
            : nullptr;
    result->moduleParamSize = phdrs[sceModuleParamIndex].p_memsz;

    // std::printf("sce_module_param: ");
    // for (auto elem : image.subspan(phdrs[sceModuleParamIndex].p_offset,
    // phdrs[sceModuleParamIndex].p_filesz)) {
    //   std::printf(" %02x", (unsigned)elem);
    // }
    // std::printf("\n");
  }

  if (tlsPhdrIndex >= 0) {
    result->tlsAlign = phdrs[tlsPhdrIndex].p_align;
    result->tlsSize = phdrs[tlsPhdrIndex].p_memsz;
    result->tlsInitSize = phdrs[tlsPhdrIndex].p_filesz;
    result->tlsInit = phdrs[tlsPhdrIndex].p_vaddr
                          ? imageBase + phdrs[tlsPhdrIndex].p_vaddr
                          : nullptr;
  }

  if (gnuEhFramePhdrIndex >= 0 && phdrs[gnuEhFramePhdrIndex].p_vaddr > 0) {
    result->ehFrameHdr = imageBase + phdrs[gnuEhFramePhdrIndex].p_vaddr;
    result->ehFrameHdrSize = phdrs[gnuEhFramePhdrIndex].p_memsz;

    struct GnuExceptionInfo {
      uint8_t version;
      uint8_t encoding;
      uint8_t fdeCount;
      uint8_t encodingTable;
      std::byte first;
    };

    auto *exinfo = reinterpret_cast<GnuExceptionInfo *>(
        image.data() + phdrs[gnuEhFramePhdrIndex].p_offset);

    if (exinfo->version != 1) {
      std::abort();
    }

    if (exinfo->fdeCount != 0x03) {
      std::abort();
    }

    if (exinfo->encodingTable != 0x3b) {
      std::abort();
    }

    std::byte *dataBuffer = nullptr;

    if (exinfo->encoding == 0x03) {
      auto offset = *reinterpret_cast<std::uint32_t *>(&exinfo->first);
      dataBuffer = imageBase + offset;
    } else if (exinfo->encoding == 0x1B) {
      auto offset = *reinterpret_cast<std::int32_t *>(&exinfo->first);
      dataBuffer = &exinfo->first + sizeof(std::int32_t) + offset;
    } else {
      std::abort();
    }

    auto *dataBufferIt = dataBuffer;
    while (true) {
      auto size = *reinterpret_cast<std::int32_t *>(dataBufferIt);
      dataBufferIt += sizeof(std::uint32_t);

      if (size == 0) {
        break;
      }

      if (size == -1) {
        size = *reinterpret_cast<std::uint64_t *>(dataBufferIt) +
               sizeof(std::uint64_t);
      }

      dataBufferIt += size;
    }

    result->ehFrame =
        imageBase + phdrs[gnuEhFramePhdrIndex].p_vaddr +
        (dataBuffer - image.data() - phdrs[gnuEhFramePhdrIndex].p_offset);
    result->ehFrameSize = dataBufferIt - dataBuffer;
  }

  if (dynamicPhdrIndex >= 0 && phdrs[dynamicPhdrIndex].p_filesz > 0) {
    auto &dynPhdr = phdrs[dynamicPhdrIndex];
    std::vector<Elf64_Dyn> dyns(dynPhdr.p_filesz / sizeof(Elf64_Dyn));
    std::memcpy(dyns.data(), image.data() + dynPhdr.p_offset,
                dyns.size() * sizeof(Elf64_Dyn));

    int sceStrtabIndex = -1;
    int sceSymtabIndex = -1;
    std::size_t sceSymtabSize = 0;
    for (auto &dyn : dyns) {
      if (dyn.d_tag == kElfDynamicTypeSceStrTab) {
        sceStrtabIndex = &dyn - dyns.data();
      } else if (dyn.d_tag == kElfDynamicTypeSceSymTab) {
        sceSymtabIndex = &dyn - dyns.data();
      } else if (dyn.d_tag == kElfDynamicTypeSceSymTabSize) {
        sceSymtabSize = dyn.d_un.d_val;
      }
    }

    auto sceStrtab = sceStrtabIndex >= 0 && sceDynlibDataPhdrIndex >= 0
                         ? reinterpret_cast<const char *>(
                               image.data() + dyns[sceStrtabIndex].d_un.d_val +
                               phdrs[sceDynlibDataPhdrIndex].p_offset)
                         : nullptr;

    auto sceDynlibData =
        sceDynlibDataPhdrIndex >= 0
            ? image.data() + phdrs[sceDynlibDataPhdrIndex].p_offset
            : nullptr;

    auto sceSymtabData =
        sceSymtabIndex >= 0 && sceDynlibData != nullptr
            ? reinterpret_cast<const Elf64_Sym *>(
                  sceDynlibData + dyns[sceSymtabIndex].d_un.d_val)
            : nullptr;

    std::unordered_map<std::uint64_t, std::size_t> idToModuleIndex;
    std::unordered_map<std::uint64_t, std::size_t> idToLibraryIndex;

    orbis::Relocation *pltRelocations = nullptr;
    std::size_t pltRelocationCount = 0;
    orbis::Relocation *nonPltRelocations = nullptr;
    std::size_t nonPltRelocationCount = 0;

    for (auto dyn : dyns) {
      // std::printf("%s: %lx", toString((OrbisElfDynamicType)dyn.d_tag),
      //             dyn.d_un.d_val);

      // if (dyn.d_tag == kElfDynamicTypeSceNeededModule ||
      //     dyn.d_tag == kElfDynamicTypeNeeded ||
      //     dyn.d_tag == kElfDynamicTypeSceOriginalFilename ||
      //     dyn.d_tag == kElfDynamicTypeSceImportLib ||
      //     dyn.d_tag == kElfDynamicTypeSceExportLib ||
      //     dyn.d_tag == kElfDynamicTypeSceModuleInfo) {
      //   std::printf(" ('%s')",
      //               sceStrtab
      //                   ? sceStrtab +
      //                   static_cast<std::uint32_t>(dyn.d_un.d_val) : "<no
      //                   strtab>");
      // }

      // std::printf("\n");

      if (dyn.d_tag == kElfDynamicTypeSceModuleInfo) {
        std::strncpy(result->moduleName,
                     sceStrtab + static_cast<std::uint32_t>(dyn.d_un.d_val),
                     sizeof(result->moduleName));
      }

      if (dyn.d_tag == kElfDynamicTypeSoName) {
        std::strncpy(result->soName,
                     sceStrtab + static_cast<std::uint32_t>(dyn.d_un.d_val),
                     sizeof(result->soName));
      }

      if (dyn.d_tag == kElfDynamicTypeNeeded) {
        auto name = std::string_view(
            sceStrtab + static_cast<std::uint32_t>(dyn.d_un.d_val));
        if (name == "libSceFreeTypeOptBm-PRX.prx") {
          // TODO
        } else if (name.ends_with(".prx")) {
          result->needed.push_back(std::string(name));
        } else if (name == "STREQUAL") {
          // HACK for broken FWs
          result->needed.push_back("libSceDolbyVision.prx");
        } else {
          std::fprintf(stderr, "Ignoring needed '%s'\n", name.data());
        }
      }

      if (dyn.d_tag == kElfDynamicTypeSceModuleInfo) {
        idToModuleIndex[dyn.d_un.d_val >> 48] = -1;
      }

      if (dyn.d_tag == kElfDynamicTypeSceNeededModule) {
        auto [it, inserted] = idToModuleIndex.try_emplace(
            dyn.d_un.d_val >> 48, result->neededModules.size());

        if (inserted) {
          result->neededModules.emplace_back();
        }

        auto &mod = result->neededModules[it->second];
        mod.name = sceStrtab + static_cast<std::uint32_t>(dyn.d_un.d_val);
        mod.isExport = false;
      } else if (dyn.d_tag == kElfDynamicTypeSceImportLib ||
                 dyn.d_tag == kElfDynamicTypeSceExportLib) {
        auto [it, inserted] = idToLibraryIndex.try_emplace(
            dyn.d_un.d_val >> 48, result->neededLibraries.size());

        if (inserted) {
          result->neededLibraries.emplace_back();
        }

        auto &lib = result->neededLibraries[it->second];

        lib.name = sceStrtab + static_cast<std::uint32_t>(dyn.d_un.d_val);
        lib.isExport = dyn.d_tag == kElfDynamicTypeSceExportLib;
      }

      switch (dyn.d_tag) {
      case kElfDynamicTypeScePltGot:
        result->pltGot =
            dyn.d_un.d_ptr
                ? reinterpret_cast<std::uint64_t *>(imageBase + dyn.d_un.d_ptr)
                : nullptr;
        break;

      case kElfDynamicTypeSceJmpRel:
        if (sceDynlibData != nullptr) {
          pltRelocations = reinterpret_cast<orbis::Relocation *>(
              sceDynlibData + dyn.d_un.d_ptr);
        }
        break;
      case kElfDynamicTypeScePltRel:
        break;
      case kElfDynamicTypeScePltRelSize:
        pltRelocationCount = dyn.d_un.d_val / sizeof(orbis::Relocation);
        break;
      case kElfDynamicTypeSceRela:
        if (sceDynlibData != nullptr) {
          nonPltRelocations = reinterpret_cast<orbis::Relocation *>(
              sceDynlibData + dyn.d_un.d_ptr);
        }
        break;
      case kElfDynamicTypeSceRelaSize:
        nonPltRelocationCount = dyn.d_un.d_val / sizeof(orbis::Relocation);
        break;
      case kElfDynamicTypeSceRelaEnt:
        break;

      case kElfDynamicTypeInit:
        result->initProc = imageBase + dyn.d_un.d_ptr;
        break;
      case kElfDynamicTypeFini:
        result->finiProc = imageBase + dyn.d_un.d_ptr;
        break;
      }
    }

    if (sceSymtabData != nullptr && sceSymtabSize / sizeof(Elf64_Sym) > 0) {
      auto sceSymtab =
          std::span(sceSymtabData, sceSymtabSize / sizeof(Elf64_Sym));

      result->symbols.reserve(sceSymtab.size());

      for (auto &sym : sceSymtab) {
        auto visibility = ELF64_ST_VISIBILITY(sym.st_other);
        auto type = ELF64_ST_TYPE(sym.st_info);
        auto bind = ELF64_ST_BIND(sym.st_info);
        orbis::Symbol symbol{
            .address = sym.st_value,
            .size = sym.st_size,
            .visibility = static_cast<orbis::SymbolVisibility>(visibility),
            .bind = static_cast<orbis::SymbolBind>(bind),
            .type = static_cast<orbis::SymbolType>(type),
        };

        if (sceStrtab != nullptr && sym.st_name != 0) {
          auto fullName = std::string_view(sceStrtab + sym.st_name);
          if (auto hashPos = fullName.find('#');
              hashPos != std::string_view::npos) {
            std::string_view module;
            std::string_view library;
            std::string_view name;

            name = fullName.substr(0, hashPos);
            auto moduleLibary = fullName.substr(hashPos + 1);

            hashPos = moduleLibary.find('#');

            if (hashPos == std::string_view::npos) {
              std::abort();
            }

            library = moduleLibary.substr(0, hashPos);
            module = moduleLibary.substr(hashPos + 1);

            auto libaryNid = *decodeNid(library);
            auto moduleNid = *decodeNid(module);

            symbol.libraryIndex = idToLibraryIndex.at(libaryNid);
            symbol.moduleIndex = idToModuleIndex.at(moduleNid);
            symbol.id = *decodeNid(name);
          } else if (auto nid = decodeNid(fullName)) {
            symbol.id = *nid;
            symbol.libraryIndex = -1;
            symbol.moduleIndex = -1;
          } else {
            std::printf("ignored: (%s) - %lx\n",
                        sceStrtab ? sceStrtab +
                                        static_cast<std::uint32_t>(sym.st_name)
                                  : "<no strtab>",
                        sym.st_value);

            continue;
          }
        }

        result->symbols.push_back(symbol);
      }
    }

    if (pltRelocations != nullptr && pltRelocationCount > 0) {
      result->pltRelocations.reserve(pltRelocationCount);
      for (auto rel : std::span(pltRelocations, pltRelocationCount)) {
        result->pltRelocations.push_back(rel);
      }
    }

    if (nonPltRelocations != nullptr && nonPltRelocationCount > 0) {
      result->nonPltRelocations.reserve(nonPltRelocationCount);
      for (auto rel : std::span(nonPltRelocations, nonPltRelocationCount)) {
        result->nonPltRelocations.push_back(rel);
      }
    }
  }

  for (auto phdr : phdrs) {
    if (phdr.p_type == kElfProgramTypeLoad ||
        phdr.p_type == kElfProgramTypeSceRelRo) {
      auto segmentSize = utils::alignUp(phdr.p_memsz, phdr.p_align);
      ::mprotect(imageBase + phdr.p_vaddr, segmentSize, PROT_WRITE);
      std::memcpy(imageBase + phdr.p_vaddr, image.data() + phdr.p_offset,
                  phdr.p_filesz);
      std::memset(imageBase + phdr.p_vaddr + phdr.p_filesz, 0,
                  phdr.p_memsz - phdr.p_filesz);

      if (phdr.p_type == kElfProgramTypeSceRelRo) {
        phdr.p_flags |= vm::kMapProtCpuWrite; // TODO: reprotect on relocations
      }

      vm::protect(imageBase + phdr.p_vaddr, segmentSize, phdr.p_flags);

      if (phdr.p_type == kElfProgramTypeLoad) {
        if (result->segmentCount >= std::size(result->segments)) {
          std::abort();
        }

        auto &segment = result->segments[result->segmentCount++];
        segment.addr = imageBase + phdr.p_vaddr;
        segment.size = phdr.p_memsz;
        segment.prot = phdr.p_flags;
      }
    }
  }

  result->base = imageBase;
  result->size = imageSize;
  // std::printf("Needed modules: [");
  // for (bool isFirst = true; auto &module : result->neededModules) {
  //   if (isFirst) {
  //     isFirst = false;
  //   } else {
  //     std::printf(", ");
  //   }

  //   std::printf("'%s'", module.name.c_str());
  // }
  // std::printf("]\n");
  // std::printf("Needed libraries: [");
  // for (bool isFirst = true; auto &library : result->neededLibraries) {
  //   if (isFirst) {
  //     isFirst = false;
  //   } else {
  //     std::printf(", ");
  //   }

  //   std::printf("'%s'", library.name.c_str());
  // }
  // std::printf("]\n");

  for (auto seg : std::span(result->segments, result->segmentCount)) {
    rx::vm::protect(seg.addr, seg.size, rx::vm::kMapProtCpuAll);
  }

  result->id = process->modulesMap.insert(result);
  result->proc = process;

  std::printf("Loaded module '%s' from object '%s', id: %u, address: %p - %p\n",
              result->moduleName, result->soName, (unsigned)result->id,
              result->base, (char *)result->base + result->size);

  if (tlsPhdrIndex >= 0 /* result->tlsSize != 0 */) {
    result->tlsIndex = process->nextTlsSlot++;
  }

  return result;
}

Ref<orbis::Module> rx::linker::loadModuleFile(const char *path,
                                              orbis::Process *process) {
  orbis::Ref<IoDeviceInstance> instance;
  if (vfs::open(path, kOpenFlagReadOnly, 0, &instance).isError()) {
    return {};
  }

  auto len = instance->lseek(instance.get(), 0, SEEK_END);
  instance->lseek(instance.get(), 0, SEEK_SET);

  std::vector<std::byte> image(len);
  auto ptr = image.data();
  auto endPtr = ptr + image.size();

  while (ptr != endPtr) {
    auto result = instance->read(instance.get(), ptr, endPtr - ptr);
    if (result < 0) {
      std::fprintf(stderr, "Module file reading error\n");
      std::abort();
    }
    ptr += result;
  }

  instance->close(instance.get());

  return loadModule(image, process);
}

static Ref<orbis::Module> createSceFreeTypeFull(orbis::Process *process) {
  auto result = orbis::create<orbis::Module>();

  std::strncpy(result->soName, "libSceFreeTypeFull-PRX.prx", sizeof(result->soName) - 1);
  std::strncpy(result->moduleName, "libSceFreeType", sizeof(result->moduleName) - 1);

  result->neededLibraries.push_back({
    .name = "libSceFreeType",
    .isExport = true
  });

  for (auto dep : { "libSceFreeTypeSubFunc", "libSceFreeTypeOl", "libSceFreeTypeOt", "libSceFreeTypeOptOl", "libSceFreeTypeHinter" }) {
    result->needed.push_back(dep);
    result->needed.back() += "-PRX.prx";
  }

  for (auto needed : result->needed) {
    auto neededMod = rx::linker::loadModuleByName(needed, process);

    if (neededMod == nullptr) {
      std::fprintf(stderr, "Failed to load needed '%s' for FreeType\n", needed.c_str());
      std::abort();
    }

    result->namespaceModules.push_back(neededMod);
  }

  // TODO: load native library with module_start and module_stop
  result->initProc = reinterpret_cast<void *>(+[] {});
  result->finiProc = reinterpret_cast<void *>(+[] {});

  result->id = process->modulesMap.insert(result);
  result->proc = process;

  return result;
}

Ref<orbis::Module> rx::linker::loadModuleByName(std::string_view name,
                                                orbis::Process *process) {
  if (name.ends_with(".prx")) {
    name.remove_suffix(4);
  }
  if (name.ends_with("-PRX")) {
    name.remove_suffix(4); // TODO: implement lib scan
  }
  if (name.ends_with("-module")) {
    name.remove_suffix(7); // TODO: implement lib scan
  }

  if (auto it = g_moduleOverrideTable.find(name);
      it != g_moduleOverrideTable.end()) {
    return loadModuleFile(it->second.c_str(), process);
  }

  if (name == "libSceFreeTypeFull") {
    return createSceFreeTypeFull(process);
  }

  for (auto path : {"/system/common/lib/", "/system/priv/lib/"}) {
    auto filePath = std::string(path);
    filePath += name;
    filePath += ".sprx";

    if (auto result = rx::linker::loadModuleFile(filePath.c_str(), process)) {
      return result;
    }
  }

  return {};
}
