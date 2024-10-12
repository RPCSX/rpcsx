#include "linker.hpp"
#include "io-device.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/module/Module.hpp"
#include "orbis/stat.hpp"
#include "orbis/uio.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include <bit>
#include <crypto/sha1.h>
#include <elf.h>
#include <filesystem>
#include <map>
#include <orbis/thread/Process.hpp>
#include <rx/align.hpp>
#include <sys/mman.h>
#include <unordered_map>

using orbis::utils::Ref;

std::uint64_t monoPimpAddress;

static std::vector<std::byte> unself(const std::byte *image, std::size_t size) {
  struct [[gnu::packed]] Header {
    std::uint32_t magic;
    std::uint32_t unk;
    std::uint8_t category;
    std::uint8_t programType;
    std::uint16_t padding;
    std::uint16_t headerSize;
    std::uint16_t signSize;
    std::uint32_t fileSize;
    std::uint32_t padding2;
    std::uint16_t segmentCount;
    std::uint16_t unk1;
    std::uint32_t padding3;
  };
  static_assert(sizeof(Header) == 0x20);

  struct [[gnu::packed]] Segment {
    std::uint64_t flags;
    std::uint64_t offset;
    std::uint64_t encryptedSize;
    std::uint64_t decryptedSize;
  };
  static_assert(sizeof(Segment) == 0x20);

  auto header = std::bit_cast<Header *>(image);
  auto segments = std::bit_cast<Segment *>(image + sizeof(Header));

  auto elfOffset = sizeof(Header) + sizeof(Segment) * header->segmentCount;
  std::vector<std::byte> result;
  result.reserve(header->fileSize);
  result.resize(sizeof(Elf64_Ehdr));

  auto ehdr = std::bit_cast<Elf64_Ehdr *>(image + elfOffset);
  auto phdrs = std::bit_cast<Elf64_Phdr *>(image + elfOffset + ehdr->e_phoff);
  std::memcpy(result.data(), ehdr, sizeof(Elf64_Ehdr));

  auto phdrEndOffset = ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum;
  if (result.size() < phdrEndOffset) {
    result.resize(phdrEndOffset);
  }

  for (std::size_t i = 0; i < ehdr->e_phnum; ++i) {
    std::memcpy(result.data() + ehdr->e_phoff + ehdr->e_phentsize * i,
                image + elfOffset + ehdr->e_phoff + ehdr->e_phentsize * i,
                sizeof(Elf64_Phdr));
  }

  for (std::size_t i = 0; i < header->segmentCount; ++i) {
    auto &segment = segments[i];
    if ((segment.flags & 0x7fb) != 0 ||
        segment.decryptedSize != segment.encryptedSize) {
      std::fprintf(stderr, "Unsupported self segment (%lx)\n", segment.flags);
      std::abort();
    }

    if (~segment.flags & 0x800) {
      continue;
    }

    auto &phdr = phdrs[segment.flags >> 20];

    auto endOffset = phdr.p_offset + segment.decryptedSize;
    if (result.size() < endOffset) {
      result.resize(endOffset);
    }

    std::memcpy(result.data() + phdr.p_offset, image + segment.offset,
                segment.decryptedSize);
  }

  return result;
}

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
  Ref<orbis::Module> result{orbis::knew<orbis::Module>()};

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

  std::uint64_t endAddress = 0;
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
          std::min(baseAddress, rx::alignDown(phdr.p_vaddr, phdr.p_align));
      endAddress = std::max(
          endAddress, rx::alignUp(phdr.p_vaddr + phdr.p_memsz, vm::kPageSize));
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
          std::min(baseAddress, rx::alignDown(phdr.p_vaddr, phdr.p_align));
      endAddress = std::max(
          endAddress, rx::alignUp(phdr.p_vaddr + phdr.p_memsz, vm::kPageSize));
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

  auto imageSize = endAddress - baseAddress;

  auto imageBase = reinterpret_cast<std::byte *>(
      vm::map(reinterpret_cast<void *>(baseAddress),
                  rx::alignUp(imageSize, vm::kPageSize), 0,
                  vm::kMapFlagPrivate | vm::kMapFlagAnonymous |
                      (baseAddress ? vm::kMapFlagFixed : 0)));

  if (imageBase == MAP_FAILED) {
    std::abort();
  }

  result->entryPoint = header.e_entry
                           ? reinterpret_cast<std::uintptr_t>(
                                 imageBase - baseAddress + header.e_entry)
                           : 0;

  if (interpPhdrIndex >= 0) {
    result->interp = reinterpret_cast<const char *>(
        image.data() + phdrs[interpPhdrIndex].p_offset);
  }

  if (sceProcParamIndex >= 0) {
    result->processParam =
        phdrs[sceProcParamIndex].p_vaddr
            ? reinterpret_cast<void *>(imageBase - baseAddress +
                                       phdrs[sceProcParamIndex].p_vaddr)
            : nullptr;
    result->processParamSize = phdrs[sceProcParamIndex].p_memsz;
  }

  if (sceModuleParamIndex >= 0) {
    result->moduleParam =
        phdrs[sceModuleParamIndex].p_vaddr
            ? reinterpret_cast<void *>(imageBase - baseAddress +
                                       phdrs[sceModuleParamIndex].p_vaddr)
            : nullptr;
    result->moduleParamSize = phdrs[sceModuleParamIndex].p_memsz;
  }

  if (tlsPhdrIndex >= 0) {
    result->tlsAlign = phdrs[tlsPhdrIndex].p_align;
    result->tlsSize = phdrs[tlsPhdrIndex].p_memsz;
    result->tlsInitSize = phdrs[tlsPhdrIndex].p_filesz;
    result->tlsInit =
        phdrs[tlsPhdrIndex].p_vaddr
            ? imageBase - baseAddress + phdrs[tlsPhdrIndex].p_vaddr
            : nullptr;
  }

  if (gnuEhFramePhdrIndex >= 0 && phdrs[gnuEhFramePhdrIndex].p_vaddr > 0) {
    result->ehFrameHdr =
        imageBase - baseAddress + phdrs[gnuEhFramePhdrIndex].p_vaddr;
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
      dataBuffer = imageBase - baseAddress + offset;
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
        imageBase - baseAddress + phdrs[gnuEhFramePhdrIndex].p_vaddr +
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
    int strtabIndex = -1;
    int symtabIndex = -1;
    int hashIndex = -1;
    int sceSymtabSizeIndex = -1;
    for (auto &dyn : dyns) {
      if (dyn.d_tag == kElfDynamicTypeSceStrTab) {
        sceStrtabIndex = &dyn - dyns.data();
      } else if (dyn.d_tag == kElfDynamicTypeSceSymTab) {
        sceSymtabIndex = &dyn - dyns.data();
      } else if (dyn.d_tag == kElfDynamicTypeSceSymTabSize) {
        sceSymtabSizeIndex = &dyn - dyns.data();
      } else if (dyn.d_tag == kElfDynamicTypeStrTab) {
        strtabIndex = &dyn - dyns.data();
      } else if (dyn.d_tag == kElfDynamicTypeSymTab) {
        symtabIndex = &dyn - dyns.data();
      } else if (dyn.d_tag == kElfDynamicTypeHash) {
        hashIndex = &dyn - dyns.data();
      }
    }

    auto dynTabOffsetGet = [&](std::uint64_t address) {
      if (sceSymtabIndex < 0 || sceStrtabIndex < 0) {
        for (auto &phdr : phdrs) {
          if (phdr.p_vaddr <= address &&
              address < phdr.p_vaddr + phdr.p_memsz) {
            return phdr.p_offset + address - phdr.p_vaddr;
          }
        }
      }

      return address;
    };

    auto sceStrtab =
        sceStrtabIndex >= 0 && sceDynlibDataPhdrIndex >= 0
            ? reinterpret_cast<const char *>(
                  image.data() +
                  dynTabOffsetGet(dyns[sceStrtabIndex].d_un.d_val) +
                  phdrs[sceDynlibDataPhdrIndex].p_offset)
            : nullptr;

    auto strtab = sceStrtab;

    if (strtab == nullptr && strtabIndex >= 0) {
      strtab = reinterpret_cast<const char *>(
          image.data() + dynTabOffsetGet(dyns[strtabIndex].d_un.d_val));
    }

    auto sceDynlibData =
        sceDynlibDataPhdrIndex >= 0
            ? image.data() + phdrs[sceDynlibDataPhdrIndex].p_offset
            : nullptr;

    auto sceSymtabData =
        sceSymtabIndex >= 0 && sceDynlibData != nullptr
            ? reinterpret_cast<const Elf64_Sym *>(
                  sceDynlibData + dyns[sceSymtabIndex].d_un.d_val)
            : nullptr;

    auto symtab = sceSymtabData;
    auto symtabSize =
        sceSymtabSizeIndex >= 0
            ? dyns[sceSymtabSizeIndex].d_un.d_val / sizeof(Elf64_Sym)
            : 0;

    if (symtab == nullptr && symtabIndex >= 0) {
      if (hashIndex < 0) {
        std::fprintf(stderr, "SYMTAB without HASH!\n");
        std::abort();
      }

      symtab = reinterpret_cast<const Elf64_Sym *>(
          image.data() + dynTabOffsetGet(dyns[symtabIndex].d_un.d_val));
      symtabSize = *reinterpret_cast<const std::uint32_t *>(
          image.data() + dynTabOffsetGet(dyns[hashIndex].d_un.d_val) +
          sizeof(std::uint32_t));
    }

    std::unordered_map<std::uint64_t, std::size_t> idToModuleIndex;
    std::unordered_map<std::uint64_t, std::size_t> idToLibraryIndex;

    orbis::Relocation *pltRelocations = nullptr;
    std::size_t pltRelocationCount = 0;
    orbis::Relocation *nonPltRelocations = nullptr;
    std::size_t nonPltRelocationCount = 0;

    for (auto dyn : dyns) {
      if (dyn.d_tag == kElfDynamicTypeSceModuleInfo) {
        std::strncpy(result->moduleName,
                     sceStrtab + static_cast<std::uint32_t>(dyn.d_un.d_val),
                     sizeof(result->moduleName));
      } else if (dyn.d_tag == kElfDynamicTypeSceModuleAttr) {
        result->attributes = dyn.d_un.d_val;
      }

      if (dyn.d_tag == kElfDynamicTypeSoName) {
        std::strncpy(result->soName,
                     strtab + static_cast<std::uint32_t>(dyn.d_un.d_val),
                     sizeof(result->soName));
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
        mod.name = strtab + static_cast<std::uint32_t>(dyn.d_un.d_val);
        mod.attr = static_cast<std::uint16_t>(dyn.d_un.d_val >> 32);
        mod.isExport = false;
      } else if (dyn.d_tag == kElfDynamicTypeSceImportLib ||
                 dyn.d_tag == kElfDynamicTypeSceExportLib) {
        auto [it, inserted] = idToLibraryIndex.try_emplace(
            dyn.d_un.d_val >> 48, result->neededLibraries.size());

        if (inserted) {
          result->neededLibraries.emplace_back();
        }

        auto &lib = result->neededLibraries[it->second];

        lib.name = strtab + static_cast<std::uint32_t>(dyn.d_un.d_val);
        lib.isExport = dyn.d_tag == kElfDynamicTypeSceExportLib;
      } else if (dyn.d_tag == kElfDynamicTypeSceExportLibAttr ||
                 dyn.d_tag == kElfDynamicTypeSceImportLibAttr) {
        auto [it, inserted] = idToLibraryIndex.try_emplace(
            dyn.d_un.d_val >> 48, result->neededLibraries.size());

        if (inserted) {
          result->neededLibraries.emplace_back();
        }

        auto &lib = result->neededLibraries[it->second];

        lib.attr = dyn.d_un.d_val & ((static_cast<std::uint64_t>(1) << 48) - 1);
      }

      switch (dyn.d_tag) {
      case kElfDynamicTypeScePltGot:
        result->pltGot = dyn.d_un.d_ptr
                             ? reinterpret_cast<std::uint64_t *>(
                                   imageBase - baseAddress + dyn.d_un.d_ptr)
                             : nullptr;
        break;

      case kElfDynamicTypeSceJmpRel:
        if (sceDynlibData != nullptr) {
          pltRelocations = reinterpret_cast<orbis::Relocation *>(
              sceDynlibData + dyn.d_un.d_ptr);
        } else {
          pltRelocations = reinterpret_cast<orbis::Relocation *>(
              image.data() + dyn.d_un.d_ptr);
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
        } else {
          nonPltRelocations = reinterpret_cast<orbis::Relocation *>(
              image.data() + dyn.d_un.d_ptr);
        }
        break;
      case kElfDynamicTypeSceRelaSize:
        nonPltRelocationCount = dyn.d_un.d_val / sizeof(orbis::Relocation);
        break;
      case kElfDynamicTypeSceRelaEnt:
        break;

      case kElfDynamicTypeInit:
        result->initProc = imageBase - baseAddress + dyn.d_un.d_ptr;
        break;
      case kElfDynamicTypeFini:
        result->finiProc = imageBase - baseAddress + dyn.d_un.d_ptr;
        break;
      }
    }

    if (symtab != nullptr && symtabSize > 0) {
      auto sceSymtab = std::span(symtab, symtabSize);

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

        if (symbol.address) {
          symbol.address -= baseAddress;
        }

        if (strtab != nullptr && sym.st_name != 0) {
          auto fullName = std::string_view(strtab + sym.st_name);
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
            if (name == "5JrIq4tzVIo") {
              monoPimpAddress = symbol.address + (std::uint64_t)imageBase;
              std::fprintf(stderr, "mono_pimp address = %lx\n",
                           monoPimpAddress);
            }
          } else if (auto nid = decodeNid(fullName)) {
            symbol.id = *nid;
            symbol.libraryIndex = -1;
            symbol.moduleIndex = -1;
          } else {
            symbol.id =
                encodeFid(strtab + static_cast<std::uint32_t>(sym.st_name));
            symbol.libraryIndex = -1;
            symbol.moduleIndex = -1;
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
      auto segmentEnd = rx::alignUp(phdr.p_vaddr + phdr.p_memsz, vm::kPageSize);
      auto segmentBegin =
          rx::alignDown(phdr.p_vaddr - baseAddress, phdr.p_align);
      auto segmentSize = segmentEnd - segmentBegin;
      ::mprotect(imageBase + segmentBegin, segmentSize, PROT_WRITE);
      std::memcpy(imageBase + phdr.p_vaddr - baseAddress,
                  image.data() + phdr.p_offset, phdr.p_filesz);

      if (phdr.p_type == kElfProgramTypeSceRelRo) {
        phdr.p_flags |= vm::kMapProtCpuWrite; // TODO: reprotect on relocations
      }

      vm::protect(imageBase + segmentBegin, segmentSize,
                  phdr.p_flags & (vm::kMapProtCpuAll | vm::kMapProtGpuAll));

      if (phdr.p_type == kElfProgramTypeLoad) {
        if (result->segmentCount >= std::size(result->segments)) {
          std::abort();
        }

        auto &segment = result->segments[result->segmentCount++];
        segment.addr = imageBase + segmentBegin;
        segment.size = phdr.p_memsz;
        segment.prot = phdr.p_flags;
      }
    }
  }

  result->base = imageBase - baseAddress;
  result->size = imageSize + baseAddress;
  result->phdrAddress = phdrPhdrIndex >= 0 ? phdrs[phdrPhdrIndex].p_vaddr
                                           : baseAddress + header.e_phoff;
  result->phNum = header.e_phnum;
  result->proc = process;

  std::printf("Loaded module '%s' (%lx) from object '%s', address: %p - %p\n",
              result->moduleName, (unsigned long)result->attributes,
              result->soName, imageBase, (char *)imageBase + result->size);
  for (auto mod : result->neededModules) {
    std::printf("  needed module '%s' (%lx)\n", mod.name.c_str(),
                (unsigned long)mod.attr);
  }

  for (auto lib : result->neededLibraries) {
    std::printf("  needed library '%s' (%lx), kind %s\n", lib.name.c_str(),
                (unsigned long)lib.attr, lib.isExport ? "export" : "import");
  }

  if (tlsPhdrIndex >= 0 /* result->tlsSize != 0 */) {
    result->tlsIndex = process->nextTlsSlot++;
  }

  return result;
}

static Ref<orbis::Module> loadModuleFileImpl(std::string_view path,
                                             orbis::Thread *thread) {
  orbis::Ref<orbis::File> instance;
  if (vfs::open(path, kOpenFlagReadOnly, 0, &instance, thread).isError()) {
    return {};
  }

  orbis::Stat fileStat;
  if (instance->ops->stat(instance.get(), &fileStat, nullptr) !=
      orbis::ErrorCode{}) {
    return {};
  }

  auto len = fileStat.size;

  std::vector<std::byte> image(len);
  auto ptr = image.data();
  orbis::IoVec ioVec{
      .base = ptr,
      .len = static_cast<std::uint64_t>(len),
  };
  orbis::Uio io{
      .offset = 0,
      .iov = &ioVec,
      .iovcnt = 1,
      .resid = 0,
      .segflg = orbis::UioSeg::SysSpace,
      .rw = orbis::UioRw::Read,
      .td = thread,
  };

  while (io.offset < image.size()) {
    ioVec = {
        .base = ptr + io.offset,
        .len = image.size() - io.offset,
    };
    auto result = instance->ops->read(instance.get(), &io, thread);
    if (result != orbis::ErrorCode{}) {
      std::fprintf(stderr, "Module file reading error\n");
      std::abort();
    }
  }

  if (image[0] != std::byte{'\x7f'} || image[1] != std::byte{'E'} ||
      image[2] != std::byte{'L'} || image[3] != std::byte{'F'}) {
    image = unself(image.data(), image.size());

    // std::ofstream(
    //     std::filesystem::path(path).filename().replace_extension("elf"),
    //     std::ios::binary)
    //     .write((const char *)image.data(), image.size());
  }

  return rx::linker::loadModule(image, thread->tproc);
}

Ref<orbis::Module> rx::linker::loadModuleFile(std::string_view path,
                                              orbis::Thread *thread) {
  if (auto result = loadModuleFileImpl(path, thread)) {
    return result;
  }

  if (path.ends_with(".sprx")) {
    path.remove_suffix(4);

    if (auto result = loadModuleFileImpl(std::string(path) + "prx", thread)) {
      return result;
    }

    return nullptr;
  }

  if (path.ends_with(".prx")) {
    path.remove_suffix(3);

    if (auto result = loadModuleFileImpl(std::string(path) + "sprx", thread)) {
      return result;
    }

    return nullptr;
  }

  return nullptr;
}
