#include "ops.hpp"
#include "align.hpp"
#include "io-device.hpp"
#include "linker.hpp"
#include "orbis/module/ModuleHandle.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/umtx.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/Rc.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <thread>
#include <unistd.h>

using namespace orbis;

extern "C" void __register_frame(const void *);

namespace {
static std::pair<SysResult, Ref<Module>>
loadPrx(orbis::Thread *thread, std::string_view name, bool relocate,
        std::map<std::string, Module *, std::less<>> &loadedObjects,
        std::map<std::string, Module *, std::less<>> &loadedModules,
        std::string_view expectedName) {
  if (auto it = loadedObjects.find(name); it != loadedObjects.end()) {
    return {{}, it->second};
  }

  auto module = rx::linker::loadModuleFile(name, thread->tproc);

  if (module == nullptr) {
    if (!expectedName.empty()) {
      loadedObjects[std::string(expectedName)] = nullptr;
    }

    return {ErrorCode::NOENT, {}};
  }

  if (!expectedName.empty() && expectedName != module->soName) {
    if (module->soName[0] != '\0') {
      std::fprintf(stderr,
                   "Module name mismatch, expected '%s', loaded '%s' (%s)\n",
                   std::string(expectedName).c_str(), module->soName,
                   module->moduleName);
      // std::abort();
    }

    std::strncpy(module->soName, std::string(expectedName).c_str(),
                 sizeof(module->soName));
    if (module->soName[sizeof(module->soName) - 1] != '\0') {
      std::fprintf(stderr, "Too big needed name\n");
      std::abort();
    }
  }

  loadedObjects[module->soName] = module.get();
  if (loadedModules.try_emplace(module->moduleName, module.get()).second) {
    std::printf("Setting '%s' as '%s' module\n", module->soName,
                module->moduleName);
  }

  for (auto &needed : module->needed) {
    auto [result, neededModule] =
        loadPrx(thread, needed, relocate, loadedObjects, loadedModules, needed);

    if (result.isError() || neededModule == nullptr) {
      std::fprintf(stderr, "Needed '%s' not found\n", needed.c_str());
    }
  }

  module->importedModules.reserve(module->neededModules.size());

  for (auto mod : module->neededModules) {
    if (auto it = loadedModules.find(std::string_view(mod.name));
        it != loadedModules.end()) {
      module->importedModules.emplace_back(it->second);
      continue;
    }

    std::fprintf(stderr, "Not found needed module '%s' for object '%s'\n",
                 mod.name.c_str(), module->soName);
    module->importedModules.push_back({});
  }

  if (relocate) {
    auto result = module->relocate(thread->tproc);
    if (result.isError()) {
      return {result, module};
    }
  }

  module->id = thread->tproc->modulesMap.insert(module);
  std::fprintf(stderr, "'%s' ('%s') was loaded with id '%u'\n",
               module->moduleName, module->soName, (unsigned)module->id);
  return {{}, module};
}

static std::pair<SysResult, Ref<Module>>
loadPrx(orbis::Thread *thread, std::string_view name, bool relocate) {
  std::map<std::string, Module *, std::less<>> loadedObjects;
  std::map<std::string, Module *, std::less<>> loadedModules;

  for (auto [id, module] : thread->tproc->modulesMap) {
    loadedObjects[module->soName] = module;
    loadedModules[module->moduleName] = module;
  }

  return loadPrx(thread, name, relocate, loadedObjects, loadedModules, {});
};

orbis::SysResult mmap(orbis::Thread *thread, orbis::caddr_t addr,
                      orbis::size_t len, orbis::sint prot, orbis::sint flags,
                      orbis::sint fd, orbis::off_t pos) {
  auto result = (void *)-1;
  if (fd == -1) {
    result = rx::vm::map(addr, len, prot, flags);
  } else {
    Ref<IoDeviceInstance> handle =
        static_cast<IoDeviceInstance *>(thread->tproc->fileDescriptors.get(fd));
    if (handle == nullptr) {
      return ErrorCode::BADF;
    }

    if (handle->mmap != nullptr) {
      result = handle->mmap(handle.get(), addr, len, prot, flags, pos);
    } else {
      ORBIS_LOG_FATAL("unimplemented mmap", fd, (void *)addr, len, prot, flags,
                      pos);
      result = rx::vm::map(addr, len, prot, flags);
    }
  }

  if (result == (void *)-1) {
    return ErrorCode::NOMEM;
  }

  thread->retval[0] = reinterpret_cast<std::uint64_t>(result);
  return {};
}

orbis::SysResult munmap(orbis::Thread *, orbis::ptr<void> addr,
                        orbis::size_t len) {
  if (rx::vm::unmap(addr, len)) {
    return {};
  }
  return ErrorCode::INVAL;
}

orbis::SysResult msync(orbis::Thread *thread, orbis::ptr<void> addr,
                       orbis::size_t len, orbis::sint flags) {
  return {};
}

orbis::SysResult mprotect(orbis::Thread *thread, orbis::ptr<const void> addr,
                          orbis::size_t len, orbis::sint prot) {
  if (!rx::vm::protect((void *)addr, len, prot)) {
    return ErrorCode::INVAL;
  }
  return {};
}

orbis::SysResult minherit(orbis::Thread *thread, orbis::ptr<void> addr,
                          orbis::size_t len, orbis::sint inherit) {
  return ErrorCode::INVAL;
}

orbis::SysResult madvise(orbis::Thread *thread, orbis::ptr<void> addr,
                         orbis::size_t len, orbis::sint behav) {
  return ErrorCode::INVAL;
}

orbis::SysResult mincore(orbis::Thread *thread, orbis::ptr<const void> addr,
                         orbis::size_t len, orbis::ptr<char> vec) {
  return ErrorCode::INVAL;
}

orbis::SysResult mlock(orbis::Thread *thread, orbis::ptr<const void> addr,
                       orbis::size_t len) {
  return {};
}
orbis::SysResult mlockall(orbis::Thread *thread, orbis::sint how) { return {}; }
orbis::SysResult munlockall(orbis::Thread *thread) { return {}; }
orbis::SysResult munlock(orbis::Thread *thread, orbis::ptr<const void> addr,
                         orbis::size_t len) {
  return {};
}
orbis::SysResult virtual_query(orbis::Thread *thread,
                               orbis::ptr<const void> addr, orbis::sint flags,
                               orbis::ptr<void> info, orbis::ulong infoSize) {
  if (infoSize != sizeof(rx::vm::VirtualQueryInfo)) {
    return ErrorCode::INVAL;
  }

  if (!rx::vm::virtualQuery(addr, flags, (rx::vm::VirtualQueryInfo *)info)) {
    return ErrorCode::FAULT;
  }
  return {};
}

orbis::SysResult open(orbis::Thread *thread, orbis::ptr<const char> path,
                      orbis::sint flags, orbis::sint mode) {
  orbis::Ref<IoDeviceInstance> instance;
  auto result = rx::vfs::open(path, flags, mode, &instance);
  if (result.isError()) {
    ORBIS_LOG_WARNING("Failed to open file", path, result.value());
    return result;
  }

  auto fd = thread->tproc->fileDescriptors.insert(instance);
  thread->retval[0] = fd;
  ORBIS_LOG_WARNING("File opened", path, fd);
  return {};
}

orbis::SysResult socket(orbis::Thread *thread, orbis::ptr<const char> name,
                        orbis::sint domain, orbis::sint type,
                        orbis::sint protocol) {
  orbis::Ref<IoDeviceInstance> instance;
  auto error = createSocket(name, domain, type, protocol, &instance);
  if (error != ErrorCode{}) {
    ORBIS_LOG_WARNING("Failed to open socket", name, int(error));
    return error;
  }

  auto fd = thread->tproc->fileDescriptors.insert(instance);
  thread->retval[0] = fd;
  ORBIS_LOG_WARNING("Socket opened", name, fd);
  return {};
}

orbis::SysResult close(orbis::Thread *thread, orbis::sint fd) {
  if (fd == 0) {
    return {};
  }
  if (!thread->tproc->fileDescriptors.close(fd)) {
    return ErrorCode::BADF;
  }

  ORBIS_LOG_WARNING("fd closed", fd);
  return {};
}

// clang-format off
#define IOCPARM_SHIFT 13 /* number of bits for ioctl size */
#define IOCPARM_MASK ((1 << IOCPARM_SHIFT) - 1) /* parameter length mask */
#define IOCPARM_LEN(x) (((x) >> 16) & IOCPARM_MASK)
#define IOCBASECMD(x) ((x) & ~(IOCPARM_MASK << 16))
#define IOCGROUP(x) (((x) >> 8) & 0xff)

#define IOCPARM_MAX (1 << IOCPARM_SHIFT) /* max size of ioctl */
#define IOC_VOID 0x20000000              /* no parameters */
#define IOC_OUT 0x40000000               /* copy out parameters */
#define IOC_IN 0x80000000                /* copy in parameters */
#define IOC_INOUT (IOC_IN | IOC_OUT)
#define IOC_DIRMASK (IOC_VOID | IOC_OUT | IOC_IN)

#define _IOC(inout, group, num, len)                                           \
  ((unsigned long)((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | \
                   (num)))
#define _IO(g, n) _IOC(IOC_VOID, (g), (n), 0)
#define _IOWINT(g, n) _IOC(IOC_VOID, (g), (n), sizeof(int))
#define _IOR(g, n, t) _IOC(IOC_OUT, (g), (n), sizeof(t))
#define _IOW(g, n, t) _IOC(IOC_IN, (g), (n), sizeof(t))
/* this should be _IORW, but stdio got there first */
#define _IOWR(g, n, t) _IOC(IOC_INOUT, (g), (n), sizeof(t))
// clang-format on

static std::string iocGroupToString(unsigned iocGroup) {
  if (iocGroup >= 128) {
    const char *sceGroups[] = {
        "DEV",
        "DMEM",
        "GC",
        "DCE",
        "UVD",
        "VCE",
        "DBGGC",
        "TWSI",
        "MDBG",
        "DEVENV",
        "AJM",
        "TRACE",
        "IBS",
        "MBUS",
        "HDMI",
        "CAMERA",
        "FAN",
        "THERMAL",
        "PFS",
        "ICC_CONFIG",
        "IPC",
        "IOSCHED",
        "ICC_INDICATOR",
        "EXFATFS",
        "ICC_NVS",
        "DVE",
        "ICC_POWER",
        "AV_CONTROL",
        "ICC_SC_CONFIGURATION",
        "ICC_DEVICE_POWER",
        "SSHOT",
        "DCE_SCANIN",
        "FSCTRL",
        "HMD",
        "SHM",
        "PHYSHM",
        "HMDDFU",
        "BLUETOOTH_HID",
        "SBI",
        "S3DA",
        "SPM",
        "BLOCKPOOL",
        "SDK_EVENTLOG",
    };

    if (iocGroup - 127 >= std::size(sceGroups)) {
      return "'?'";
    }

    return sceGroups[iocGroup - 127];
  }

  if (isprint(iocGroup)) {
    return "'" + std::string(1, (char)iocGroup) + "'";
  }

  return "'?'";
}

static void printIoctl(unsigned long arg) {
  std::printf("0x%lx { IO%s%s %lu(%s), %lu, %lu }\n", arg,
              arg & IOC_OUT ? "R" : "", arg & IOC_IN ? "W" : "", IOCGROUP(arg),
              iocGroupToString(IOCGROUP(arg)).c_str(), arg & 0xFF,
              IOCPARM_LEN(arg));
}

static void ioctlToStream(std::ostream &stream, unsigned long arg) {
  stream << "0x" << std::hex << arg << " { IO";

  if ((arg & IOC_OUT) != 0) {
    stream << 'R';
  }

  if ((arg & IOC_IN) != 0) {
    stream << 'W';
  }
  if ((arg & IOC_VOID) != 0) {
    stream << 'i';
  }

  stream << " 0x" << IOCGROUP(arg);
  stream << "('" << iocGroupToString(IOCGROUP(arg)) << "'), ";
  stream << std::dec << (arg & 0xFF) << ", " << IOCPARM_LEN(arg) << " }";
}

static std::string ioctlToString(unsigned long arg) {
  std::ostringstream stream;
  ioctlToStream(stream, arg);
  return std::move(stream).str();
}

orbis::SysResult ioctl(orbis::Thread *thread, orbis::sint fd, orbis::ulong com,
                       orbis::caddr_t argp) {
  std::printf("ioctl: %d %s\n", (int)fd, ioctlToString(com).c_str());

  Ref<IoDeviceInstance> handle =
      static_cast<IoDeviceInstance *>(thread->tproc->fileDescriptors.get(fd));
  if (handle == nullptr) {
    return ErrorCode::BADF;
  }

  if (handle->ioctl == nullptr) {
    return ErrorCode::NOTSUP;
  }

  auto result = handle->ioctl(handle.get(), com, argp);

  if (result < 0) {
    // TODO
    return ErrorCode::IO;
  }

  thread->retval[0] = result;
  return {};
}
orbis::SysResult write(orbis::Thread *thread, orbis::sint fd,
                       orbis::ptr<const void> data, orbis::ulong size) {
  Ref<IoDeviceInstance> handle =
      static_cast<IoDeviceInstance *>(thread->tproc->fileDescriptors.get(fd));
  if (handle == nullptr) {
    return ErrorCode::BADF;
  }

  auto result = handle->write(handle.get(), data, size);

  if (result < 0) {
    // TODO
    return ErrorCode::IO;
  }

  thread->retval[0] = result;
  return {};
}
orbis::SysResult read(orbis::Thread *thread, orbis::sint fd,
                      orbis::ptr<void> data, orbis::ulong size) {
  Ref<IoDeviceInstance> handle =
      static_cast<IoDeviceInstance *>(thread->tproc->fileDescriptors.get(fd));
  if (handle == nullptr) {
    return ErrorCode::BADF;
  }

  auto result = handle->read(handle.get(), data, size);

  if (result < 0) {
    // TODO
    return ErrorCode::IO;
  }

  thread->retval[0] = result;
  return {};
}
orbis::SysResult pread(orbis::Thread *thread, orbis::sint fd,
                       orbis::ptr<void> data, orbis::ulong size,
                       orbis::ulong offset) {
  Ref<IoDeviceInstance> handle =
      static_cast<IoDeviceInstance *>(thread->tproc->fileDescriptors.get(fd));
  if (handle == nullptr) {
    return ErrorCode::BADF;
  }

  auto prevPos = handle->lseek(handle.get(), 0, SEEK_CUR);
  handle->lseek(handle.get(), offset, SEEK_SET);
  auto result = handle->read(handle.get(), data, size);
  handle->lseek(handle.get(), prevPos, SEEK_SET);

  if (result < 0) {
    // TODO
    return ErrorCode::IO;
  }

  thread->retval[0] = result;
  return {};
}
orbis::SysResult pwrite(orbis::Thread *thread, orbis::sint fd,
                        orbis::ptr<const void> data, orbis::ulong size,
                        orbis::ulong offset) {
  return ErrorCode::NOTSUP;
}
orbis::SysResult lseek(orbis::Thread *thread, orbis::sint fd,
                       orbis::ulong offset, orbis::sint whence) {
  Ref<IoDeviceInstance> handle =
      static_cast<IoDeviceInstance *>(thread->tproc->fileDescriptors.get(fd));
  if (handle == nullptr) {
    return ErrorCode::BADF;
  }

  auto result = handle->lseek(handle.get(), offset, whence);

  if (result < 0) {
    // TODO
    return ErrorCode::IO;
  }

  thread->retval[0] = result;
  return {};
}
orbis::SysResult ftruncate(orbis::Thread *thread, orbis::sint fd,
                           orbis::off_t length) {
  return {};
}
orbis::SysResult truncate(orbis::Thread *thread, orbis::ptr<const char> path,
                          orbis::off_t length) {
  return ErrorCode::NOTSUP;
}

orbis::SysResult dynlib_get_obj_member(orbis::Thread *thread,
                                       orbis::ModuleHandle handle,
                                       orbis::uint64_t index,
                                       orbis::ptr<orbis::ptr<void>> addrp) {
  std::lock_guard lock(thread->tproc->mtx);
  auto module = thread->tproc->modulesMap.get(handle);

  if (module == nullptr) {
    return ErrorCode::INVAL;
  }

  switch (index) {
  case 1:
    *addrp = module->initProc;
    return {};

  case 8:
    *addrp = module->moduleParam;
    return {};
  }

  return ErrorCode::INVAL;
}

ptr<char> findSymbolById(orbis::Module *module, std::uint64_t id) {
  for (auto sym : module->symbols) {
    if (sym.id == id && sym.bind != orbis::SymbolBind::Local) {
      return sym.address != 0 ? (ptr<char>)module->base + sym.address : 0;
    }
  }

  return nullptr;
}

orbis::SysResult dynlib_dlsym(orbis::Thread *thread, orbis::ModuleHandle handle,
                              orbis::ptr<const char> symbol,
                              orbis::ptr<orbis::ptr<void>> addrp) {
  std::printf("sys_dynlib_dlsym(%u, '%s')\n", (unsigned)handle, symbol);
  std::lock_guard lock(thread->tproc->mtx);
  auto module = thread->tproc->modulesMap.get(handle);

  if (module == nullptr) {
    return ErrorCode::INVAL;
  }

  std::printf("sys_dynlib_dlsym(%s (%s), '%s')\n", module->soName,
              module->moduleName, symbol);

  std::string_view symView(symbol);

  if (auto nid = rx::linker::decodeNid(symView)) {
    if (auto addr = findSymbolById(module, *nid)) {
      *addrp = addr;
      return {};
    }
  }

  std::printf("sys_dynlib_dlsym(%s (%s), '%s')\n", module->soName,
              module->moduleName,
              rx::linker::encodeNid(rx::linker::encodeFid(symView)).string);

  if (auto addr = findSymbolById(module, rx::linker::encodeFid(symView))) {
    *addrp = addr;
    return {};
  }

  return ErrorCode::NOENT;
}
orbis::SysResult dynlib_do_copy_relocations(orbis::Thread *thread) {
  // TODO
  return {};
}

orbis::SysResult dynlib_load_prx(orbis::Thread *thread,
                                 orbis::ptr<const char> name,
                                 orbis::uint64_t arg1,
                                 orbis::ptr<ModuleHandle> pHandle,
                                 orbis::uint64_t arg3) {
  std::printf("sys_dynlib_load_prx: %s\n", name);

  std::lock_guard lock(thread->tproc->mtx);

  char _name[256];
  auto errorCode = ureadString(_name, sizeof(_name), name);
  if (errorCode != ErrorCode{}) {
    return errorCode;
  }

  auto [result, module] = loadPrx(thread, _name, true);
  if (result.isError()) {
    return result;
  }

  *pHandle = module->id;
  return {};
}
orbis::SysResult dynlib_unload_prx(orbis::Thread *thread,
                                   orbis::ModuleHandle handle) {
  return ErrorCode::NOTSUP;
}

SysResult thr_create(orbis::Thread *thread, orbis::ptr<struct ucontext> ctxt,
                     ptr<orbis::slong> arg, orbis::sint flags) {
  return ErrorCode::NOTSUP;
}
SysResult thr_new(orbis::Thread *thread, orbis::ptr<thr_param> param,
                  orbis::sint param_size) {
  thr_param _param;
  auto result = uread(_param, param);
  if (result != ErrorCode{}) {
    return result;
  }

  auto proc = thread->tproc;
  std::lock_guard lock(proc->mtx);
  auto [baseId, childThread] = proc->threadsMap.emplace();
  childThread->tproc = proc;
  childThread->tid = proc->pid + baseId;
  childThread->state = orbis::ThreadState::RUNQ;
  childThread->stackStart = _param.stack_base;
  childThread->stackEnd = _param.stack_base + _param.stack_size;
  childThread->fsBase = reinterpret_cast<std::uintptr_t>(_param.tls_base);

  result = uwrite(_param.parent_tid, slong(childThread->tid));

  if (result != ErrorCode{}) {
    return result;
  }

  // FIXME: implement scheduler

  ORBIS_LOG_NOTICE("Starting child thread", childThread->tid,
                   childThread->stackStart);

  auto stdthr = std::thread{[=, childThread = Ref<Thread>(childThread)] {
    stack_t ss;

    auto sigStackSize = std::max<std::size_t>(
        SIGSTKSZ, ::utils::alignUp(8 * 1024 * 1024, sysconf(_SC_PAGE_SIZE)));

    ss.ss_sp = malloc(sigStackSize);
    if (ss.ss_sp == NULL) {
      perror("malloc");
      ::exit(EXIT_FAILURE);
    }

    ss.ss_size = sigStackSize;
    ss.ss_flags = 0;

    if (sigaltstack(&ss, NULL) == -1) {
      perror("sigaltstack");
      ::exit(EXIT_FAILURE);
    }

    static_cast<void>(
        uwrite(_param.child_tid, slong(childThread->tid))); // TODO: verify
    auto context = new ucontext_t{};

    context->uc_mcontext.gregs[REG_RDI] =
        reinterpret_cast<std::uintptr_t>(_param.arg);
    context->uc_mcontext.gregs[REG_RSI] =
        reinterpret_cast<std::uintptr_t>(_param.arg);
    context->uc_mcontext.gregs[REG_RSP] =
        reinterpret_cast<std::uintptr_t>(childThread->stackEnd);
    context->uc_mcontext.gregs[REG_RIP] =
        reinterpret_cast<std::uintptr_t>(_param.start_func);

    childThread->context = context;
    childThread->state = orbis::ThreadState::RUNNING;
    rx::thread::invoke(childThread.get());
  }};

  if (pthread_setname_np(stdthr.native_handle(),
                         std::to_string(childThread->tid).c_str())) {
    perror("pthread_setname_np");
  }

  childThread->handle = std::move(stdthr);
  return {};
}
SysResult thr_exit(orbis::Thread *thread, orbis::ptr<orbis::slong> state) {
  std::printf("Requested exit of thread %u, state %p\n", (unsigned)thread->tid,
              state);
  if (state != nullptr) {
    static_cast<void>(uwrite(state, (orbis::slong)1));
    umtx_wake(thread, state, INT_MAX);
  }

  // FIXME: implement exit
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }
  return ErrorCode::NOTSUP;
}
SysResult thr_kill(orbis::Thread *thread, orbis::slong id, orbis::sint sig) {
  return ErrorCode::NOTSUP;
}
SysResult thr_kill2(orbis::Thread *thread, orbis::pid_t pid, orbis::slong id,
                    orbis::sint sig) {
  return ErrorCode::NOTSUP;
}
SysResult thr_suspend(orbis::Thread *thread,
                      orbis::ptr<const orbis::timespec> timeout) {
  return ErrorCode::NOTSUP;
}
SysResult thr_wake(orbis::Thread *thread, orbis::slong id) {
  return ErrorCode::NOTSUP;
}
SysResult thr_set_name(orbis::Thread *thread, orbis::slong id,
                       orbis::ptr<const char> name) {
  return ErrorCode::NOTSUP;
}
orbis::SysResult exit(orbis::Thread *thread, orbis::sint status) {
  std::printf("Requested exit with status %d\n", status);
  std::exit(status);
}

SysResult processNeeded(Thread *thread) {
  std::map<std::string, Module *, std::less<>> loadedObjects;
  std::map<std::string, Module *, std::less<>> loadedModules;
  std::set<std::string> allNeeded;

  for (auto [id, module] : thread->tproc->modulesMap) {
    loadedObjects[module->soName] = module;
    loadedModules[module->moduleName] = module;
    for (auto &needed : module->needed) {
      allNeeded.insert(std::string(needed));
    }
  }

  for (auto needed : allNeeded) {
    auto [result, neededModule] =
        loadPrx(thread, needed, false, loadedObjects, loadedModules, needed);

    if (result.isError() || neededModule == nullptr) {
      std::fprintf(stderr, "Needed '%s' not found\n", needed.c_str());
      continue;
    }
  }

  for (auto [id, module] : thread->tproc->modulesMap) {
    module->importedModules.reserve(module->neededModules.size());

    for (auto mod : module->neededModules) {
      if (auto it = loadedModules.find(std::string_view(mod.name));
          it != loadedModules.end()) {
        module->importedModules.emplace_back(it->second);
        continue;
      }

      std::fprintf(stderr, "Not found needed module '%s' for object '%s'\n",
                   mod.name.c_str(), module->soName);
      module->importedModules.push_back({});
    }

    module->relocate(thread->tproc);
  }

  return {};
}

SysResult registerEhFrames(Thread *thread) {
  for (auto [id, module] : thread->tproc->modulesMap) {
    if (module->ehFrame != nullptr) {
      __register_frame(module->ehFrame);
    }
  }

  return {};
}
} // namespace

ProcessOps rx::procOpsTable = {
    .mmap = mmap,
    .munmap = munmap,
    .msync = msync,
    .mprotect = mprotect,
    .minherit = minherit,
    .madvise = madvise,
    .mincore = mincore,
    .mlock = mlock,
    .mlockall = mlockall,
    .munlockall = munlockall,
    .munlock = munlock,
    .virtual_query = virtual_query,
    .open = open,
    .socket = socket,
    .close = close,
    .ioctl = ioctl,
    .write = write,
    .read = read,
    .pread = pread,
    .pwrite = pwrite,
    .lseek = lseek,
    .ftruncate = ftruncate,
    .truncate = truncate,
    .dynlib_get_obj_member = dynlib_get_obj_member,
    .dynlib_dlsym = dynlib_dlsym,
    .dynlib_do_copy_relocations = dynlib_do_copy_relocations,
    .dynlib_load_prx = dynlib_load_prx,
    .dynlib_unload_prx = dynlib_unload_prx,
    .thr_create = thr_create,
    .thr_new = thr_new,
    .thr_exit = thr_exit,
    .thr_kill = thr_kill,
    .thr_kill2 = thr_kill2,
    .thr_suspend = thr_suspend,
    .thr_wake = thr_wake,
    .thr_set_name = thr_set_name,
    .exit = exit,
    .processNeeded = processNeeded,
    .registerEhFrames = registerEhFrames,
};
