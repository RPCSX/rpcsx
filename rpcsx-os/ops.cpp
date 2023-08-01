#include "ops.hpp"
#include "align.hpp"
#include "backtrace.hpp"
#include "io-device.hpp"
#include "iodev/blockpool.hpp"
#include "iodev/dmem.hpp"
#include "linker.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/module/ModuleHandle.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/umtx.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/utils/Rc.hpp"
#include "orbis/vm.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
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
  if (auto it = loadedObjects.find(expectedName); it != loadedObjects.end()) {
    return {{}, it->second};
  }

  if (auto it = loadedObjects.find(name); it != loadedObjects.end()) {
    return {{}, it->second};
  }

  auto module = rx::linker::loadModuleFile(name, thread);

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
loadPrx(orbis::Thread *thread, std::string_view path, bool relocate) {
  std::map<std::string, Module *, std::less<>> loadedObjects;
  std::map<std::string, Module *, std::less<>> loadedModules;

  for (auto [id, module] : thread->tproc->modulesMap) {
    loadedObjects[module->soName] = module;
    loadedModules[module->moduleName] = module;
  }

  std::string expectedName;
  if (auto sep = path.rfind('/'); sep != std::string_view::npos) {
    auto tmpExpectedName = path.substr(sep + 1);

    if (tmpExpectedName.ends_with(".sprx")) {
      tmpExpectedName.remove_suffix(5);
    }

    expectedName += tmpExpectedName;
    expectedName += ".prx";
  }

  return loadPrx(thread, path, relocate, loadedObjects, loadedModules,
                 expectedName);
}

orbis::SysResult mmap(orbis::Thread *thread, orbis::caddr_t addr,
                      orbis::size_t len, orbis::sint prot, orbis::sint flags,
                      orbis::sint fd, orbis::off_t pos) {
  if (fd == -1) {
    auto result = rx::vm::map(addr, len, prot, flags);
    if (result == (void *)-1) {
      return ErrorCode::NOMEM;
    }

    thread->retval[0] = reinterpret_cast<std::uint64_t>(result);
    return {};
  }

  auto file = thread->tproc->fileDescriptors.get(fd);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (file->ops->mmap == nullptr) {
    ORBIS_LOG_FATAL("unimplemented mmap", fd, (void *)addr, len, prot, flags,
                    pos);
    return mmap(thread, addr, len, prot, flags, -1, 0);
  }

  void *maddr = addr;
  auto result = file->ops->mmap(file, &maddr, len, prot, flags, pos, thread);

  if (result != ErrorCode{}) {
    return result;
  }

  thread->retval[0] = reinterpret_cast<std::uint64_t>(maddr);
  return {};
}

orbis::SysResult dmem_mmap(orbis::Thread *thread, orbis::caddr_t addr,
                           orbis::size_t len, orbis::sint memoryType,
                           orbis::sint prot, sint flags,
                           orbis::off_t directMemoryStart) {
  auto dmem = static_cast<DmemDevice *>(orbis::g_context.dmemDevice.get());
  void *address = addr;
  auto result =
      dmem->mmap(&address, len, memoryType, prot, flags, directMemoryStart);
  if (result != ErrorCode{}) {
    return result;
  }

  thread->retval[0] = reinterpret_cast<std::uint64_t>(address);
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

orbis::SysResult
query_memory_protection(orbis::Thread *thread, orbis::ptr<void> address,
                        orbis::ptr<MemoryProtection> protection) {
  if (rx::vm::queryProtection(address, &protection->startAddress,
                              &protection->endAddress, &protection->prot)) {
    return {};
  }
  return ErrorCode::INVAL;
}

orbis::SysResult open(orbis::Thread *thread, orbis::ptr<const char> path,
                      orbis::sint flags, orbis::sint mode,
                      orbis::Ref<orbis::File> *file) {
  return rx::vfs::open(path, flags, mode, file, thread);
}

orbis::SysResult shm_open(orbis::Thread *thread, const char *path,
                          orbis::sint flags, orbis::sint mode,
                          orbis::Ref<orbis::File> *file) {
  auto dev = static_cast<IoDevice *>(orbis::g_context.shmDevice.get());
  return dev->open(file, path, flags, mode, thread);
}

orbis::SysResult blockpool_open(orbis::Thread *thread,
                                orbis::Ref<orbis::File> *file) {
  auto dev = static_cast<IoDevice *>(orbis::g_context.blockpoolDevice.get());
  return dev->open(file, nullptr, 0, 0, thread);
}

orbis::SysResult blockpool_map(orbis::Thread *thread, orbis::caddr_t addr,
                               orbis::size_t len, orbis::sint prot,
                               orbis::sint flags) {
  auto blockpool =
      static_cast<BlockPoolDevice *>(orbis::g_context.blockpoolDevice.get());
  void *address = addr;
  auto result = blockpool->map(&address, len, prot, flags, thread);
  if (result != ErrorCode{}) {
    return result;
  }

  thread->retval[0] = reinterpret_cast<std::uint64_t>(address);
  return {};
}
orbis::SysResult blockpool_unmap(orbis::Thread *thread, orbis::caddr_t addr,
                                 orbis::size_t len) {
  auto blockpool =
      static_cast<BlockPoolDevice *>(orbis::g_context.blockpoolDevice.get());
  return blockpool->unmap(addr, len, thread);
}

orbis::SysResult socket(orbis::Thread *thread, orbis::ptr<const char> name,
                        orbis::sint domain, orbis::sint type,
                        orbis::sint protocol, Ref<File> *file) {
  return createSocket(file, name, domain, type, protocol);
}

orbis::SysResult shm_unlink(orbis::Thread *thread, const char *path) {
  auto dev = static_cast<IoDevice *>(orbis::g_context.shmDevice.get());
  return dev->unlink(path, thread);
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

void where(Thread *thread) {
  rx::printStackTrace((ucontext_t *)thread->context, thread, 2);
}
} // namespace

ProcessOps rx::procOpsTable = {
    .mmap = mmap,
    .dmem_mmap = dmem_mmap,
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
    .query_memory_protection = query_memory_protection,
    .open = open,
    .shm_open = shm_open,
    .blockpool_open = blockpool_open,
    .blockpool_map = blockpool_map,
    .blockpool_unmap = blockpool_unmap,
    .socket = socket,
    .shm_unlink = shm_unlink,
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
    .where = where,
};
