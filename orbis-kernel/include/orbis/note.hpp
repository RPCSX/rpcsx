#pragma once

#include "KernelAllocator.hpp"
#include "orbis-config.hpp"
#include "utils/SharedMutex.hpp"
#include <mutex>
#include <set>

namespace orbis {
struct File;
static constexpr auto kEvFiltRead = -1;
static constexpr auto kEvFiltWrite = -2;
static constexpr auto kEvFiltAio = -3;
static constexpr auto kEvFiltVnode = -4;
static constexpr auto kEvFiltProc = -5;
static constexpr auto kEvFiltSignal = -6;
static constexpr auto kEvFiltTimer = -7;
static constexpr auto kEvFiltFs = -9;
static constexpr auto kEvFiltLio = -10;
static constexpr auto kEvFiltUser = -11;
static constexpr auto kEvFiltPolling = -12;
static constexpr auto kEvFiltDisplay = -13;
static constexpr auto kEvFiltGraphicsCore = -14;
static constexpr auto kEvFiltHrTimer = -15;
static constexpr auto kEvFiltUvdTrap = -16;
static constexpr auto kEvFiltVceTrap = -17;
static constexpr auto kEvFiltSdmaTrap = -18;
static constexpr auto kEvFiltRegEv = -19;
static constexpr auto kEvFiltGpuException = -20;
static constexpr auto kEvFiltGpuSystemException = -21;
static constexpr auto kEvFiltGpuDbgGcEv = -22;
static constexpr auto kEvFiltSysCount = 22;

// actions
static constexpr auto kEvAdd = 0x0001;
static constexpr auto kEvDelete = 0x0002;
static constexpr auto kEvEnable = 0x0004;
static constexpr auto kEvDisable = 0x0008;

// flags
static constexpr auto kEvOneshot = 0x0010;
static constexpr auto kEvClear = 0x0020;
static constexpr auto kEvReceipt = 0x0040;
static constexpr auto kEvDispatch = 0x0080;
static constexpr auto kEvSysFlags = 0xf000;
static constexpr auto kEvFlag1 = 0x2000;

static constexpr auto kEvEof = 0x8000;
static constexpr auto kEvError = 0x4000;

// kEvFiltUser
static constexpr auto kNoteFFNop = 0x00000000;
static constexpr auto kNoteFFAnd = 0x40000000;
static constexpr auto kNoteFFOr = 0x80000000;
static constexpr auto kNoteFFCopy = 0xc0000000;
static constexpr auto kNoteFFCtrlMask = 0xc0000000;
static constexpr auto kNoteFFlagsMask = 0x00ffffff;
static constexpr auto kNoteTrigger = 0x01000000;

// kEvFiltProc
static constexpr auto kNoteExit = 0x80000000;
static constexpr auto kNoteFork = 0x40000000;
static constexpr auto kNoteExec = 0x20000000;

struct KEvent {
  uintptr_t ident;
  sshort filter;
  ushort flags;
  uint fflags;
  intptr_t data;
  ptr<void> udata;
};

struct KQueue;
struct KNote {
  shared_mutex mutex;
  Ref<KQueue> queue;
  Ref<File> file;
  KEvent event{};
  bool enabled = true;
  bool triggered = false;
  void *linked = nullptr; // TODO: use Ref<>

  ~KNote();
};

struct EventEmitter : orbis::RcBase {
  shared_mutex mutex;
  std::set<KNote *, std::less<>, kallocator<KNote *>> notes;

  void emit(uint filter, uint fflags = 0, intptr_t data = 0);
};
} // namespace orbis
