#pragma once

#include "error/ErrorCode.hpp"
#include "orbis-config.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace orbis {
struct IoVec {
  void *base;        // Base address
  std::uint64_t len; // Length
};

enum class UioRw : std::uint8_t { Read, Write };

// Segment flag values
enum class UioSeg : std::uint8_t {
  UserSpace, // from user data space
  SysSpace,  // from system space
  NoCopy     // don't copy, already in object
};

struct Uio {
  std::uint64_t offset;
  IoVec *iov;
  std::uint32_t iovcnt;
  std::int64_t resid;
  UioSeg segflg;
  UioRw rw;
  void *td;

  ErrorCode write(const void *data, std::size_t size) {
    auto pos = reinterpret_cast<const std::byte *>(data);
    auto end = pos + size;

    for (auto vec : std::span(iov, iovcnt)) {
      if (pos >= end) {
        break;
      }

      auto nextPos = std::min(pos + vec.len, end);
      ORBIS_RET_ON_ERROR(uwriteRaw(vec.base, pos, nextPos - pos));
      offset += nextPos - pos;
      pos = nextPos;
    }

    return {};
  }

  template <typename T> ErrorCode write(const T &object) {
    return write(&object, sizeof(T));
  }
};
} // namespace orbis
