
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <elf.h>
#include <fstream>
#include <vector>

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

int main(int argc, const char *argv[]) {
  if (argc != 3) {
    return 1;
  }

  std::vector<std::byte> image;
  {
    std::ifstream f(argv[1], std::ios::binary | std::ios::ate);

    if (!f) {
      return 1;
    }

    std::size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    image.resize(size);
    f.read((char *)image.data(), image.size());
    if (!f) {
      return 1;
    }
  }

  auto result = unself(image.data(), image.size());

  {
    std::ofstream f(argv[2], std::ios::binary);
    f.write((char *)result.data(), result.size());
    if (!f) {
      return 1;
    }
  }

  return 0;
}