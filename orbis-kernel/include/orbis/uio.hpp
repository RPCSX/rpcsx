#pragma once

#include <cstdint>

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
  std::int32_t iovcnt;
  std::int32_t resid;
  UioSeg segflg;
  UioRw rw;
  void *td;
};
} // namespace orbis
