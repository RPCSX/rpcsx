#include "ops.hpp"
#include "io-device.hpp"
#include "linker.hpp"
#include "orbis/module/ModuleHandle.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Rc.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include <chrono>
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

    result = handle->mmap(handle.get(), addr, len, prot, flags, pos);
  }

  if (result == (void *)-1) {
    return ErrorCode::NOMEM;
  }

  thread->retval[0] = reinterpret_cast<std::uint64_t>(result);
  return {};
}

orbis::SysResult munmap(orbis::Thread *thread, orbis::ptr<void> addr,
                        orbis::size_t len) {
  return ErrorCode::INVAL;
}

orbis::SysResult msync(orbis::Thread *thread, orbis::ptr<void> addr,
                       orbis::size_t len, orbis::sint flags) {
  return {};
}

orbis::SysResult mprotect(orbis::Thread *thread, orbis::ptr<const void> addr,
                          orbis::size_t len, orbis::sint prot) {
  rx::vm::protect((void *)addr, len, prot);
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
  std::printf("sys_open(%s)\n", path);
  orbis::Ref<IoDeviceInstance> instance;
  auto result = rx::vfs::open(path, flags, mode, &instance);
  if (result.isError()) {
    return result;
  }

  thread->retval[0] = thread->tproc->fileDescriptors.insert(instance);
  return {};
}

orbis::SysResult close(orbis::Thread *thread, orbis::sint fd) {
  if (!thread->tproc->fileDescriptors.remove(fd)) {
    return ErrorCode::BADF;
  }

  return {};
}

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
  ((unsigned long)((inout) | (((len)&IOCPARM_MASK) << 16) | ((group) << 8) |   \
                   (num)))
#define _IO(g, n) _IOC(IOC_VOID, (g), (n), 0)
#define _IOWINT(g, n) _IOC(IOC_VOID, (g), (n), sizeof(int))
#define _IOR(g, n, t) _IOC(IOC_OUT, (g), (n), sizeof(t))
#define _IOW(g, n, t) _IOC(IOC_IN, (g), (n), sizeof(t))
/* this should be _IORW, but stdio got there first */
#define _IOWR(g, n, t) _IOC(IOC_INOUT, (g), (n), sizeof(t))

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
  std::printf("ioctl: %s\n", ioctlToString(com).c_str());

  Ref<IoDeviceInstance> handle =
      static_cast<IoDeviceInstance *>(thread->tproc->fileDescriptors.get(fd));
  if (handle == nullptr) {
    return ErrorCode::BADF;
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
  return ErrorCode::NOTSUP;
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
  return ErrorCode::NOTSUP;
}
orbis::SysResult truncate(orbis::Thread *thread, orbis::ptr<const char> path,
                          orbis::off_t length) {
  return ErrorCode::NOTSUP;
}

orbis::SysResult dynlib_get_obj_member(orbis::Thread *thread,
                                       orbis::ModuleHandle handle,
                                       orbis::uint64_t index,
                                       orbis::ptr<orbis::ptr<void>> addrp) {
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
      return (ptr<char>)module->base + sym.address;
    }
  }

  return nullptr;
}

orbis::SysResult dynlib_dlsym(orbis::Thread *thread, orbis::ModuleHandle handle,
                              orbis::ptr<const char> symbol,
                              orbis::ptr<orbis::ptr<void>> addrp) {
  std::printf("sys_dynlib_dlsym(%u, '%s')\n", (unsigned)handle, symbol);
  auto module = thread->tproc->modulesMap.get(handle);

  if (module == nullptr) {
    return ErrorCode::INVAL;
  }

  std::printf("sys_dynlib_dlsym(%s (%s), '%s')\n", module->soName, module->moduleName, symbol);

  std::string_view symView(symbol);

  if (auto nid = rx::linker::decodeNid(symView)) {
    if (auto addr = findSymbolById(module, *nid)) {
      *addrp = addr;
      return {};
    }
  }

  std::printf("sys_dynlib_dlsym(%s (%s), '%s')\n", module->soName, module->moduleName, rx::linker::encodeNid(rx::linker::encodeFid(symView)).string);

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
  auto module = rx::linker::loadModuleFile(name, thread->tproc);
  thread->tproc->ops->processNeeded(thread);
  auto result = module->relocate(thread->tproc);
  if (result.isError()) {
    thread->tproc->modulesMap.remove(module->id);
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
  return {}; // FIXME: remove when we ready for MT
  auto _param = uread(param);

  auto proc = thread->tproc;
  auto [baseId, childThread] = proc->threadsMap.emplace();
  childThread->tproc = proc;
  childThread->tid = proc->pid + baseId;
  childThread->state = orbis::ThreadState::RUNQ;
  childThread->stackStart = _param.stack_base;
  childThread->stackEnd = _param.stack_base + _param.stack_size;
  childThread->fsBase = reinterpret_cast<std::uintptr_t>(_param.tls_base);

  uwrite(_param.parent_tid, slong(childThread->tid));

  // FIXME: implement scheduler

  std::printf("Starting child thread %lu\n", (long)(proc->pid + baseId));

  std::thread {
    [=, childThread = Ref<Thread>(childThread)] {
      uwrite(_param.child_tid, slong(childThread->tid));
      auto context = new ucontext_t{};

      context->uc_mcontext.gregs[REG_RDI] = reinterpret_cast<std::uintptr_t>(_param.arg);
      context->uc_mcontext.gregs[REG_RSI] = reinterpret_cast<std::uintptr_t>(_param.arg);
      context->uc_mcontext.gregs[REG_RSP] = reinterpret_cast<std::uintptr_t>(childThread->stackEnd);
      context->uc_mcontext.gregs[REG_RIP] = reinterpret_cast<std::uintptr_t>(_param.start_func);

      childThread->context = context;
      childThread->state = orbis::ThreadState::RUNNING;
      rx::thread::invoke(childThread.get());
    }
  }.detach();

  return {};
}
SysResult thr_exit(orbis::Thread *thread, orbis::ptr<orbis::slong> state) {
  std::printf("Requested exit of thread %u, state %p\n", (unsigned)thread->tid, state);
  // FIXME: do sys_mtx(WAKE) if state is not null 

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
                      orbis::ptr<const timespec> timeout) {
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
  while (true) {
    std::set<std::string, std::less<>> allNeededObjects;
    auto proc = thread->tproc;
    std::map<std::string, Module *, std::less<>> loadedModules;
    std::map<std::string, Module *, std::less<>> loadedObjects;

    for (auto [id, module] : proc->modulesMap) {
      for (const auto &object : module->needed) {
        allNeededObjects.emplace(object.begin(), object.end());
      }

      loadedModules[module->moduleName] = module;
      loadedObjects[module->soName] = module;
    }

    bool hasLoadedNeeded = false;

    for (auto &needed : allNeededObjects) {
      if (auto it = loadedObjects.find(needed); it != loadedObjects.end()) {
        continue;
      }

      auto neededModule = rx::linker::loadModuleByName(needed, proc);

      if (neededModule == nullptr) {
        std::fprintf(stderr, "Needed '%s' not found\n", needed.c_str());
        continue;
      }

      if (neededModule->soName != needed) {
        if (neededModule->soName[0] != '\0') {
          std::fprintf(
              stderr, "Module name mismatch, expected '%s', loaded '%s' (%s)\n",
              needed.c_str(), neededModule->soName, neededModule->moduleName);
          // std::abort();
        }

        std::strncpy(neededModule->soName, needed.c_str(),
                     sizeof(neededModule->soName));
        if (neededModule->soName[sizeof(neededModule->soName) - 1] != '\0') {
          std::fprintf(stderr, "Too big needed name\n");
          std::abort();
        }
      }

      hasLoadedNeeded = true;
    }

    if (!hasLoadedNeeded) {
      thread->tproc->modulesMap.walk([&loadedModules](ModuleHandle modId,
                                                      Module *module) {
        // std::printf("Module '%s' has id %u\n", module->name,
        // (unsigned)modId);

        module->importedModules.clear();
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
      });

      break;
    }
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
