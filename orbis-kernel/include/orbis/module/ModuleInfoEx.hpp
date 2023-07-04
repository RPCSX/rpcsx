#pragma once

#include "ModuleSegment.hpp"
#include "orbis-config.hpp"

namespace orbis {
struct ModuleInfoEx {
  uint64_t size;
  char name[256];
  uint32_t id;
  uint32_t tlsIndex;
  ptr<void> tlsInit;
  uint32_t tlsInitSize;
  uint32_t tlsSize;
  uint32_t tlsOffset;
  uint32_t tlsAlign;
  ptr<void> initProc;
  ptr<void> finiProc;
  uint64_t reserved1;
  uint64_t reserved2;
  ptr<void> ehFrameHdr;
  ptr<void> ehFrame;
  uint32_t ehFrameHdrSize;
  uint32_t ehFrameSize;
  ModuleSegment segments[4];
  uint32_t segmentCount;
  uint32_t refCount;
};
} // namespace orbis
