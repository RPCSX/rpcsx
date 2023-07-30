#pragma once
#include "../error/SysResult.hpp"
#include "../module/ModuleHandle.hpp"
#include "../thread/types.hpp"
#include "orbis-config.hpp"
#include "orbis/utils/Rc.hpp"

namespace orbis {
struct Thread;
struct Module;
struct timespec;
struct File;

struct ProcessOps {
  SysResult (*mmap)(Thread *thread, caddr_t addr, size_t len, sint prot,
                    sint flags, sint fd, off_t pos);
  SysResult (*dmem_mmap)(Thread *thread, caddr_t addr, size_t len,
                         sint memoryType, sint prot, sint flags,
                         off_t directMemoryStart);
  SysResult (*munmap)(Thread *thread, ptr<void> addr, size_t len);
  SysResult (*msync)(Thread *thread, ptr<void> addr, size_t len, sint flags);
  SysResult (*mprotect)(Thread *thread, ptr<const void> addr, size_t len,
                        sint prot);
  SysResult (*minherit)(Thread *thread, ptr<void> addr, size_t len,
                        sint inherit);
  SysResult (*madvise)(Thread *thread, ptr<void> addr, size_t len, sint behav);
  SysResult (*mincore)(Thread *thread, ptr<const void> addr, size_t len,
                       ptr<char> vec);
  SysResult (*mlock)(Thread *thread, ptr<const void> addr, size_t len);
  SysResult (*mlockall)(Thread *thread, sint how);
  SysResult (*munlockall)(Thread *thread);
  SysResult (*munlock)(Thread *thread, ptr<const void> addr, size_t len);
  SysResult (*virtual_query)(Thread *thread, ptr<const void> addr, sint flags,
                             ptr<void> info, ulong infoSize);

  SysResult (*open)(Thread *thread, ptr<const char> path, sint flags, sint mode,
                    Ref<File> *file);
  SysResult (*shm_open)(Thread *thread, const char *path, sint flags, sint mode,
                        Ref<File> *file);
  SysResult (*blockpool_open)(Thread *thread, Ref<File> *file);
  SysResult (*blockpool_map)(Thread *thread, caddr_t addr, size_t len,
                             sint prot, sint flags);
  SysResult (*blockpool_unmap)(Thread *thread, caddr_t addr, size_t len);
  SysResult (*socket)(Thread *thread, ptr<const char> name, sint domain,
                      sint type, sint protocol, Ref<File> *file);
  SysResult (*shm_unlink)(Thread *thread, const char *path);
  SysResult (*dynlib_get_obj_member)(Thread *thread, ModuleHandle handle,
                                     uint64_t index, ptr<ptr<void>> addrp);
  SysResult (*dynlib_dlsym)(Thread *thread, ModuleHandle handle,
                            ptr<const char> symbol, ptr<ptr<void>> addrp);
  SysResult (*dynlib_do_copy_relocations)(Thread *thread);
  SysResult (*dynlib_load_prx)(Thread *thread, ptr<const char> name,
                               uint64_t arg1, ptr<ModuleHandle> pHandle,
                               uint64_t arg3);
  SysResult (*dynlib_unload_prx)(Thread *thread, ModuleHandle handle);

  SysResult (*thr_create)(Thread *thread, ptr<struct ucontext> ctxt,
                          ptr<slong> arg, sint flags);
  SysResult (*thr_new)(Thread *thread, ptr<thr_param> param, sint param_size);
  SysResult (*thr_exit)(Thread *thread, ptr<slong> state);
  SysResult (*thr_kill)(Thread *thread, slong id, sint sig);
  SysResult (*thr_kill2)(Thread *thread, pid_t pid, slong id, sint sig);
  SysResult (*thr_suspend)(Thread *thread, ptr<const timespec> timeout);
  SysResult (*thr_wake)(Thread *thread, slong id);
  SysResult (*thr_set_name)(Thread *thread, slong id, ptr<const char> name);

  SysResult (*exit)(Thread *thread, sint status);

  SysResult (*processNeeded)(Thread *thread);
  SysResult (*registerEhFrames)(Thread *thread);

  void (*where)(Thread *);
};
} // namespace orbis
