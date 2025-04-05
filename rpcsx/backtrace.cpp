#include "backtrace.hpp"
#include "thread.hpp"
#include "xbyak/xbyak.h"
#include <cinttypes>
#include <cstdio>
#include <libunwind.h>
#include <link.h>
#include <orbis/thread/Process.hpp>
#include <sys/ucontext.h>
#include <ucontext.h>

extern std::uint64_t monoPimpAddress;

static auto callGuest = [] {
  struct SetContext : Xbyak::CodeGenerator {
    SetContext() {
      mov(rbx, rsp);
      mov(rsp, rdx);
      sub(rsp, 128);
      push(rbx);
      call(rsi);
      pop(rsp);
      ret();
    }
  } static setContextStorage;

  return setContextStorage
      .getCode<const char *(*)(std::uint64_t, std::uint64_t, std::uint64_t)>();
}();

bool allowMonoDebug = false;

std::size_t rx::printAddressLocation(char *dest, std::size_t destLen,
                                     orbis::Thread *thread,
                                     std::uint64_t address) {
  if (thread == nullptr || address == 0) {
    return 0;
  }

  for (auto [id, module] : thread->tproc->modulesMap) {
    auto moduleBase = reinterpret_cast<std::uint64_t>(module->base);
    if (moduleBase > address || moduleBase + module->size <= address) {
      continue;
    }

    const char *name = "";
    if (monoPimpAddress && allowMonoDebug &&
        (std::string_view(module->soName).contains(".dll.") ||
         std::string_view(module->soName).contains(".exe."))) {
      allowMonoDebug = false;
      auto ctx = reinterpret_cast<ucontext_t *>(thread->context);
      rx::thread::setupSignalStack();
      auto prevFs = _readfsbase_u64();
      _writefsbase_u64(thread->fsBase);
      name =
          callGuest(address, monoPimpAddress, ctx->uc_mcontext.gregs[REG_RSP]);
      _writefsbase_u64(prevFs);
      allowMonoDebug = true;
    }

    return std::snprintf(dest, destLen, "%s+%#" PRIx64 " (%#" PRIx64 ") %s",
                         module->soName[0] != '\0' ? module->soName
                                                   : module->moduleName,
                         address - moduleBase, address, name);
  }

  return 0;
}

void rx::printStackTrace(ucontext_t *context, int fileno) {
  unw_cursor_t cursor;

  char buffer[1024];

  flockfile(stderr);
  if (int r = unw_init_local2(&cursor, context, UNW_INIT_SIGNAL_FRAME)) {
    int len = snprintf(buffer, sizeof(buffer), "unw_init_local: %s\n",
                       unw_strerror(r));
    write(fileno, buffer, len);
    funlockfile(stderr);
    return;
  }

  char functionName[256];

  int count = 0;
  do {
    unw_word_t ip;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);

    unw_word_t off;
    int proc_res =
        unw_get_proc_name(&cursor, functionName, sizeof(functionName), &off);

    Dl_info dlinfo;
    int dladdr_res = ::dladdr((void *)ip, &dlinfo);

    unsigned long baseAddress =
        dladdr_res != 0 ? reinterpret_cast<std::uint64_t>(dlinfo.dli_fbase) : 0;

    int len = snprintf(buffer, sizeof(buffer), "%3d: %s+%p: %s(%lx)+%#lx\n",
                       count, (dladdr_res != 0 ? dlinfo.dli_fname : "??"),
                       reinterpret_cast<void *>(ip - baseAddress),
                       (proc_res == 0 ? functionName : "??"),
                       reinterpret_cast<unsigned long>(
                           proc_res == 0 ? ip - baseAddress - off : 0),
                       static_cast<unsigned long>(proc_res == 0 ? off : 0));
    write(fileno, buffer, len);
    count++;
  } while (unw_step(&cursor) > 0 && count < 64);
  funlockfile(stderr);
}

void rx::printStackTrace(ucontext_t *context, orbis::Thread *thread,
                         int fileno) {
  unw_cursor_t cursor;

  char buffer[1024];
  flockfile(stderr);
  if (int r = unw_init_local2(&cursor, context, UNW_INIT_SIGNAL_FRAME)) {
    int len = snprintf(buffer, sizeof(buffer), "unw_init_local: %s\n",
                       unw_strerror(r));
    write(fileno, buffer, len);
    funlockfile(stderr);
    return;
  }

  int count = 0;
  char functionName[256];
  do {
    unw_word_t ip;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    std::size_t offset = 0;

    offset +=
        std::snprintf(buffer + offset, sizeof(buffer) - offset, "%3d: ", count);

    if (auto loc = printAddressLocation(buffer + offset,
                                        sizeof(buffer) - offset, thread, ip)) {
      offset += loc;
      offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "\n");
    } else {
      unw_word_t off;
      int proc_res =
          unw_get_proc_name(&cursor, functionName, sizeof(functionName), &off);

      Dl_info dlinfo;
      int dladdr_res = ::dladdr((void *)ip, &dlinfo);

      unsigned long baseAddress =
          dladdr_res != 0 ? reinterpret_cast<std::uint64_t>(dlinfo.dli_fbase)
                          : 0;

      offset = snprintf(buffer, sizeof(buffer), "%3d: %s+%p: %s(%lx)+%#lx\n",
                        count, (dladdr_res != 0 ? dlinfo.dli_fname : "??"),
                        reinterpret_cast<void *>(ip - baseAddress),
                        (proc_res == 0 ? functionName : "??"),
                        reinterpret_cast<unsigned long>(
                            proc_res == 0 ? ip - baseAddress - off : 0),
                        static_cast<unsigned long>(proc_res == 0 ? off : 0));
    }

    write(fileno, buffer, offset);
    count++;
  } while (unw_step(&cursor) > 0 && count < 64);
  funlockfile(stderr);
}
