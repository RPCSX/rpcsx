#include "ops.hpp"
#include "backtrace.hpp"
#include "io-device.hpp"
#include "io-devices.hpp"
#include "iodev/blockpool.hpp"
#include "iodev/dmem.hpp"
#include "linker.hpp"
#include "orbis-config.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/file.hpp"
#include "orbis/module/ModuleHandle.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/uio.hpp"
#include "orbis/umtx.hpp"
#include "orbis/utils/Logs.hpp"
#include "orbis/vm.hpp"
#include "rx/Rc.hpp"
#include "rx/watchdog.hpp"
#include "thread.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <linux/prctl.h>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace orbis;

extern bool allowMonoDebug;

extern "C" void __register_frame(const void *);
void setupSigHandlers();
int guestExec(orbis::Thread *mainThread,
              rx::Ref<orbis::Module> executableModule,
              std::span<std::string> argv, std::span<std::string> envp);

namespace {
static std::pair<SysResult, rx::Ref<Module>>
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

static std::pair<SysResult, rx::Ref<Module>>
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

    if (!expectedName.ends_with(".prx")) {
      expectedName += ".prx";
    }
  }

  return loadPrx(thread, path, relocate, loadedObjects, loadedModules,
                 expectedName);
}

std::string getAbsolutePath(std::string path, Thread *thread) {
  if (!path.starts_with('/')) {
    path = "/" + std::string(thread->tproc->cwd) + "/" + path;
  }

  path = std::filesystem::path(path).lexically_normal().string();

  if (!path.starts_with("/dev") &&
      !path.starts_with("/system")) { // fixme: implement devfs mount
    path = std::string(thread->tproc->root) + "/" + path;
  }

  return std::filesystem::path(path).lexically_normal().string();
}

orbis::SysResult mmap(orbis::Thread *thread, orbis::caddr_t addr,
                      orbis::size_t len, orbis::sint prot, orbis::sint flags,
                      orbis::sint fd, orbis::off_t pos) {
  if (fd == -1) {
    auto result = vm::map(addr, len, prot, flags);
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
  auto result =
      file->ops->mmap(file.get(), &maddr, len, prot, flags, pos, thread);

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
  auto result = dmem->mmap(&address, len, prot, flags, directMemoryStart);
  if (result != ErrorCode{}) {
    return result;
  }

  thread->retval[0] = reinterpret_cast<std::uint64_t>(address);
  return {};
}

orbis::SysResult munmap(orbis::Thread *, orbis::ptr<void> addr,
                        orbis::size_t len) {
  if (vm::unmap(addr, len)) {
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
  if (!vm::protect((void *)addr, len, prot)) {
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
  return {};
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
  if (infoSize != sizeof(vm::VirtualQueryInfo)) {
    return ErrorCode::INVAL;
  }

  if (!vm::virtualQuery(addr, flags, (vm::VirtualQueryInfo *)info)) {
    return ErrorCode::ACCES;
  }
  return {};
}

orbis::SysResult
query_memory_protection(orbis::Thread *thread, orbis::ptr<void> address,
                        orbis::ptr<MemoryProtection> protection) {
  if (vm::queryProtection(address, &protection->startAddress,
                          &protection->endAddress, &protection->prot)) {
    return {};
  }
  return ErrorCode::INVAL;
}

orbis::SysResult open(orbis::Thread *thread, orbis::ptr<const char> path,
                      orbis::sint flags, orbis::sint mode,
                      rx::Ref<orbis::File> *file) {
  return vfs::open(getAbsolutePath(path, thread), flags, mode, file, thread);
}

orbis::SysResult shm_open(orbis::Thread *thread, const char *path,
                          orbis::sint flags, orbis::sint mode,
                          rx::Ref<orbis::File> *file) {
  auto dev = static_cast<IoDevice *>(orbis::g_context.shmDevice.get());
  return dev->open(file, path, flags, mode, thread);
}
orbis::SysResult unlink(orbis::Thread *thread, orbis::ptr<const char> path) {
  return vfs::unlink(getAbsolutePath(path, thread), thread);
}
orbis::SysResult mkdir(Thread *thread, ptr<const char> path, sint mode) {
  ORBIS_LOG_TODO(__FUNCTION__, path, mode);
  return vfs::mkdir(getAbsolutePath(path, thread), mode, thread);
}
orbis::SysResult rmdir(Thread *thread, ptr<const char> path) {
  ORBIS_LOG_TODO(__FUNCTION__, path);
  return vfs::rmdir(getAbsolutePath(path, thread), thread);
}
orbis::SysResult rename(Thread *thread, ptr<const char> from,
                        ptr<const char> to) {
  ORBIS_LOG_TODO(__FUNCTION__, from, to);
  return vfs::rename(getAbsolutePath(from, thread), getAbsolutePath(to, thread),
                     thread);
}

orbis::SysResult blockpool_open(orbis::Thread *thread,
                                rx::Ref<orbis::File> *file) {
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
                        orbis::sint protocol, rx::Ref<File> *file) {
  return createSocket(file, name ? name : "", domain, type, protocol);
}

orbis::SysResult socketPair(orbis::Thread *thread, orbis::sint domain,
                            orbis::sint type, orbis::sint protocol,
                            rx::Ref<File> *a, rx::Ref<File> *b) {

  if (domain == 1 && type == 1 && protocol == 0) {
    int fds[2];
    if (::socketpair(domain, type, protocol, fds)) {
      return convertErrno();
    }

    *a = wrapSocket(fds[0], "", domain, type, protocol);
    *b = wrapSocket(fds[1], "", domain, type, protocol);
    return {};
  }

  auto result = wrapSocket(-1, "", domain, type, protocol);
  *a = result;
  *b = result;
  return {};
}

orbis::SysResult shm_unlink(orbis::Thread *thread, const char *path) {
  auto dev = static_cast<IoDevice *>(orbis::g_context.shmDevice.get());
  return dev->unlink(path, false, thread);
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

  default:
    ORBIS_LOG_ERROR(__FUNCTION__, index);
    thread->where();
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
    if (auto addr = findSymbolById(module.get(), *nid)) {
      *addrp = addr;
      return {};
    }
  }

  std::printf("sys_dynlib_dlsym(%s (%s), '%s')\n", module->soName,
              module->moduleName,
              rx::linker::encodeNid(rx::linker::encodeFid(symView)).string);

  if (auto addr =
          findSymbolById(module.get(), rx::linker::encodeFid(symView))) {
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

  auto path = getAbsolutePath(_name, thread);

  {
    rx::Ref<orbis::File> file;
    if (auto result = vfs::open(path, 0, 0, &file, thread); result.isError()) {
      return result;
    }
  }

  auto [result, module] = loadPrx(thread, path, true);
  if (result.isError()) {
    return result;
  }

  {
    std::map<std::string, Module *, std::less<>> loadedModules;

    for (auto [id, module] : thread->tproc->modulesMap) {
      // std::fprintf(stderr, "%u: %s\n", (unsigned)id, module->moduleName);
      loadedModules[module->moduleName] = module;
    }

    for (auto [id, module] : thread->tproc->modulesMap) {
      module->importedModules.clear();
      module->importedModules.reserve(module->neededModules.size());

      for (auto mod : module->neededModules) {
        if (auto it = loadedModules.find(std::string_view(mod.name));
            it != loadedModules.end()) {
          module->importedModules.emplace_back(it->second);
          continue;
        }

        module->importedModules.push_back({});
      }

      module->relocate(thread->tproc);
    }
  }

  *pHandle = module->id;
  return {};
}
orbis::SysResult dynlib_unload_prx(orbis::Thread *thread,
                                   orbis::ModuleHandle handle) {
  return ErrorCode::NOTSUP;
}

SysResult thr_create(orbis::Thread *thread, orbis::ptr<orbis::UContext> ctxt,
                     ptr<orbis::slong> arg, orbis::sint flags) {
  ORBIS_LOG_FATAL(__FUNCTION__, ctxt, arg, flags);
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
  std::lock_guard lockThr(childThread->mtx);
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

  ORBIS_LOG_NOTICE("Starting child thread", thread->tid, childThread->tid,
                   childThread->stackStart, _param.rtp, _param.name,
                   _param.spare[0], _param.spare[1]);

  if (_param.name != 0) {
    ORBIS_RET_ON_ERROR(
        ureadString(childThread->name, sizeof(childThread->name), _param.name));
  }

  if (_param.rtp != 0) {
    rtprio _rtp;
    ORBIS_RET_ON_ERROR(uread(_rtp, _param.rtp));
    ORBIS_LOG_NOTICE("  rtp: ", _rtp.type, _rtp.prio);
  }
  childThread->handle =
      std::thread{[=, childThread = rx::Ref<Thread>(childThread)] {
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

        childThread->hostTid = ::gettid();
        childThread->context = context;
        childThread->state = orbis::ThreadState::RUNNING;

        rx::thread::setupSignalStack();
        rx::thread::setupThisThread();
        rx::thread::invoke(childThread.get());
      }};

  std::string name = std::to_string(childThread->tid);
  if (childThread->name[0] != '0') {
    name += '-';
    name += childThread->name;
  }

  if (name.size() > 15) {
    name.resize(15);
  }
  if (pthread_setname_np(childThread->getNativeHandle(), name.c_str())) {
    perror("pthread_setname_np");
  }

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
  pthread_setname_np(pthread_self(), "dead");
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }
  return ErrorCode::NOTSUP;
}
SysResult thr_kill(orbis::Thread *thread, orbis::slong id, orbis::sint sig) {
  auto t = (std::lock_guard(thread->tproc->mtx),
            thread->tproc->threadsMap.get(id - thread->tproc->pid));
  if (t == nullptr) {
    return ErrorCode::SRCH;
  }

  ORBIS_LOG_FATAL(__FUNCTION__, id, sig, t->hostTid);
  t->sendSignal(sig);
  return {};
}

SysResult thr_kill2(orbis::Thread *thread, orbis::pid_t pid, orbis::slong id,
                    orbis::sint sig) {
  ORBIS_LOG_FATAL(__FUNCTION__, pid, id, sig);
  return ErrorCode::NOTSUP;
}
SysResult thr_suspend(orbis::Thread *thread,
                      orbis::ptr<const orbis::timespec> timeout) {
  ORBIS_LOG_FATAL(__FUNCTION__, timeout);
  return ErrorCode::NOTSUP;
}
SysResult thr_wake(orbis::Thread *thread, orbis::slong id) {
  ORBIS_LOG_FATAL(__FUNCTION__, id);
  return ErrorCode::NOTSUP;
}
SysResult sigreturn(orbis::Thread *thread,
                    orbis::ptr<orbis::UContext> context) {
  rx::thread::setContext(thread, *context);
  return {};
}
SysResult thr_set_name(orbis::Thread *thread, orbis::slong id,
                       orbis::ptr<const char> name) {
  ORBIS_LOG_WARNING(__FUNCTION__, name, id, thread->tid);
  return {};
}

orbis::SysResult unmount(orbis::Thread *thread, orbis::ptr<char> path,
                         orbis::sint flags) {
  // TODO: support other that nullfs
  ORBIS_LOG_WARNING(__FUNCTION__, path);
  thread->where();
  vfs::unlink(getAbsolutePath(path, thread), thread);
  return {};
}
orbis::SysResult nmount(orbis::Thread *thread, orbis::ptr<orbis::IoVec> iovp,
                        orbis::uint iovcnt, orbis::sint flags) {
  ORBIS_LOG_ERROR(__FUNCTION__, iovp, iovcnt, flags);

  std::string_view fstype;
  std::string fspath;
  std::string target;

  for (auto it = iovp; it < iovp + iovcnt; it += 2) {
    IoVec a;
    IoVec b;
    ORBIS_RET_ON_ERROR(uread(a, it));
    ORBIS_RET_ON_ERROR(uread(b, it + 1));

    std::string_view key((char *)a.base, a.len - 1);
    std::string_view value((char *)b.base, b.len - 1);

    if (key == "fstype") {
      fstype = value;
    } else if (key == "fspath") {
      fspath = getAbsolutePath(std::string(value), thread);
    } else if (key == "target") {
      target = getAbsolutePath(std::string(value), thread);
    }

    std::fprintf(stderr, "%s: '%s':'%s'\n", __FUNCTION__, key.data(),
                 value.data());
  }

  if (fstype == "nullfs") {
    vfs::unlinkAll(fspath, thread);
    return vfs::createSymlink(target, fspath, thread);
  }

  // TODO
  return {};
}

orbis::SysResult exit(orbis::Thread *thread, orbis::sint status) {
  std::printf("Requested exit with status %d\n", status);
  thread->tproc->exitStatus = status;
  thread->tproc->event.emit(orbis::kEvFiltProc, orbis::kNoteExit, status);
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

  for (const auto &needed : allNeeded) {
    auto [result, neededModule] =
        loadPrx(thread, needed, false, loadedObjects, loadedModules, needed);

    if (result.isError() || neededModule == nullptr) {
      std::fprintf(stderr, "Needed '%s' not found\n", needed.c_str());
      continue;
    }
  }

  for (auto [id, module] : thread->tproc->modulesMap) {
    module->importedModules.clear();
    module->importedModules.reserve(module->neededModules.size());

    for (const auto &mod : module->neededModules) {
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

SysResult fork(Thread *thread, slong flags) {
  auto childPid = g_context.allocatePid() * 10000 + 1;
  ORBIS_LOG_TODO(__FUNCTION__, flags, childPid, thread->tid);
  thread->where();
  auto flag = knew<std::atomic<bool>>();
  *flag = false;

  int hostPid = ::fork();

  if (hostPid) {
    while (*flag == false) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    rx::attachProcess(hostPid);

    kfree(flag, sizeof(*flag));

    thread->tproc->event.emit(orbis::kEvFiltProc, orbis::kNoteFork, childPid);
    thread->retval[0] = childPid;
    thread->retval[1] = 0;
    return {};
  }

  auto process = g_context.createProcess(childPid);
  process->hostPid = ::getpid();
  process->sysent = thread->tproc->sysent;
  process->onSysEnter = thread->tproc->onSysEnter;
  process->onSysExit = thread->tproc->onSysExit;
  process->ops = thread->tproc->ops;
  process->parentProcess = thread->tproc;
  process->authInfo = thread->tproc->authInfo;
  process->sdkVersion = thread->tproc->sdkVersion;
  process->type = thread->tproc->type;
  for (auto [id, mod] : thread->tproc->modulesMap) {
    if (!process->modulesMap.insert(id, mod)) {
      std::abort();
    }
  }

  if (flags & RFFDG) {
    std::lock_guard lock(thread->tproc->fileDescriptors.mutex);
    for (auto [id, mod] : thread->tproc->fileDescriptors) {
      if (!process->fileDescriptors.insert(id, mod)) {
        std::abort();
      }
    }
  }

  vm::fork(childPid);
  vfs::fork();

  *flag = true;

  auto [baseId, newThread] = process->threadsMap.emplace();
  newThread->tproc = process;
  newThread->hostTid = ::gettid();
  newThread->tid = process->pid + baseId;
  newThread->state = orbis::ThreadState::RUNNING;
  newThread->context = thread->context;
  newThread->fsBase = thread->fsBase;

  orbis::g_currentThread = newThread;
  newThread->retval[0] = 0;
  newThread->retval[1] = 1;
  newThread->nativeHandle = pthread_self();

  thread = orbis::g_currentThread;

  setupSigHandlers();
  rx::thread::initialize();
  rx::thread::setupThisThread();

  auto logFd =
      ::open(("log-" + std::to_string(thread->tproc->pid) + ".txt").c_str(),
             O_CREAT | O_TRUNC | O_WRONLY, 0666);

  dup2(logFd, 1);
  dup2(logFd, 2);

  ::close(logFd);

  return {};
}

SysResult execve(Thread *thread, ptr<char> fname, ptr<ptr<char>> argv,
                 ptr<ptr<char>> envv) {
  ORBIS_LOG_ERROR(__FUNCTION__, fname);
  thread->where();

  std::vector<std::string> _argv;
  std::vector<std::string> _envv;

  if (auto ptr = argv) {
    char *p;
    while (uread(p, ptr) == ErrorCode{} && p != nullptr) {
      ORBIS_LOG_ERROR(" argv ", p);
      _argv.push_back(p);
      ++ptr;
    }
  }

  if (auto ptr = envv) {
    char *p;
    while (uread(p, ptr) == ErrorCode{} && p != nullptr) {
      ORBIS_LOG_ERROR(" envv ", p);
      _envv.push_back(p);
      ++ptr;
    }
  }

  std::string path = getAbsolutePath(fname, thread);
  ORBIS_LOG_ERROR(__FUNCTION__, path);

  {
    auto name = path;
    if (auto slashP = name.rfind('/'); slashP != std::string::npos) {
      name = name.substr(slashP + 1);
    }

    if (name.size() > 15) {
      name.resize(15);
    }

    pthread_setname_np(pthread_self(), name.c_str());
  }
  std::printf("pid: %u\n", ::getpid());

  // if (thread->tid == 60001) {
  //     while (debuggerPresent == false) {
  //       std::this_thread::sleep_for(std::chrono::seconds(1));
  //     }
  // }
  {
    rx::Ref<File> file;
    auto result = vfs::open(path, kOpenFlagReadOnly, 0, &file, thread);
    if (result.isError()) {
      return result;
    }
  }

  vm::reset();

  thread->tproc->nextTlsSlot = 1;
  for (auto [id, mod] : thread->tproc->modulesMap) {
    thread->tproc->modulesMap.close(id);
  }

  auto executableModule = rx::linker::loadModuleFile(path, thread);

  executableModule->id = thread->tproc->modulesMap.insert(executableModule);
  thread->tproc->processParam = executableModule->processParam;
  thread->tproc->processParamSize = executableModule->processParamSize;

  thread->tproc->event.emit(orbis::kEvFiltProc, orbis::kNoteExec);

  ORBIS_LOG_ERROR(__FUNCTION__, "done");
  std::thread([&] {
    rx::thread::setupSignalStack();
    rx::thread::initialize();
    rx::thread::setupThisThread();

    thread->hostTid = ::gettid();
    thread->nativeHandle = pthread_self();
    orbis::g_currentThread = thread;

    guestExec(thread, executableModule, _argv, _envv);
  }).join();
  std::abort();
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

void block(Thread *thread) {
  if (--thread->unblocked != 0) {
    return;
  }
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGSYS);
  if (pthread_sigmask(SIG_BLOCK, &set, nullptr)) {
    perror("pthread_sigmask block");
    std::abort();
  }
  // ORBIS_LOG_ERROR("blocking thread", thread->tid,
  // orbis::g_currentThread->tid);

  std::free(thread->altStack.back());
  thread->altStack.pop_back();
  thread->sigReturns.pop_back();
}

void unblock(Thread *thread) {
  if (thread->unblocked++ != 0) {
    return;
  }

  // ORBIS_LOG_ERROR("unblocking thread", thread->tid,
  //                 orbis::g_currentThread->tid);
  rx::thread::copyContext(thread, thread->sigReturns.emplace_back(),
                          *reinterpret_cast<ucontext_t *>(thread->context));

  auto altStack = malloc(rx::thread::getSigAltStackSize());
  thread->altStack.push_back(altStack);
  rx::thread::setupSignalStack(altStack);

  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGSYS);
  if (pthread_sigmask(SIG_UNBLOCK, &set, nullptr)) {
    perror("pthread_sigmask unblock");
    std::abort();
  }
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
    .unlink = unlink,
    .mkdir = mkdir,
    .rmdir = rmdir,
    .rename = rename,
    .blockpool_open = blockpool_open,
    .blockpool_map = blockpool_map,
    .blockpool_unmap = blockpool_unmap,
    .socket = socket,
    .socketpair = socketPair,
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
    .sigreturn = sigreturn,
    .thr_set_name = thr_set_name,
    .unmount = unmount,
    .nmount = nmount,
    .fork = fork,
    .execve = execve,
    .exit = exit,
    .processNeeded = processNeeded,
    .registerEhFrames = registerEhFrames,
    .where = where,
    .unblock = unblock,
    .block = block,
};
