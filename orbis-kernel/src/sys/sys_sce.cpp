#include "sys/sys_sce.hpp"
#include "KernelContext.hpp"
#include "error.hpp"
#include "evf.hpp"
#include "module/ModuleInfo.hpp"
#include "module/ModuleInfoEx.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"
#include <chrono>

orbis::SysResult orbis::sys_netcontrol(Thread *thread, sint fd, uint op,
                                       ptr<void> buf, uint nbuf) {
  return {};
}
orbis::SysResult orbis::sys_netabort(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_netgetsockinfo(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_socketex(Thread *thread, ptr<const char> name,
                                     sint domain, sint type, sint protocol) {
  return {};
}
orbis::SysResult orbis::sys_socketclose(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_netgetiflist(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_kqueueex(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mtypeprotect(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_regmgr_call(Thread *thread, uint32_t op,
                                        uint32_t id, ptr<void> result,
                                        ptr<void> value, uint64_t type) {
  if (op == 25) {
    struct nonsys_int {
      union {
        uint64_t encoded_id;
        struct {
          uint8_t data[4];
          uint8_t table;
          uint8_t index;
          uint16_t checksum;
        } encoded_id_parts;
      };
      uint32_t unk;
      uint32_t value;
    };

    auto int_value = reinterpret_cast<nonsys_int *>(value);
    ORBIS_LOG_TODO(
        __FUNCTION__, int_value->encoded_id,
        int_value->encoded_id_parts.data[0],
        int_value->encoded_id_parts.data[1],
        int_value->encoded_id_parts.data[2],
        int_value->encoded_id_parts.data[3], int_value->encoded_id_parts.table,
        int_value->encoded_id_parts.index, int_value->encoded_id_parts.checksum,
        int_value->unk, int_value->value);
  }

  return {};
}
orbis::SysResult orbis::sys_jitshm_create(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_jitshm_alias(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dl_get_list(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dl_get_info(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dl_notify_event(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_evf_create(Thread *thread, ptr<const char[32]> name,
                                       sint attrs, ptr<struct evFlag> evf) {
  ORBIS_LOG_WARNING(__FUNCTION__, name, attrs, evf);
  if (name == nullptr || evf != nullptr) {
    return ErrorCode::INVAL;
  }

  if (attrs & ~(kEvfAttrSingle | kEvfAttrMulti | kEvfAttrThPrio |
                kEvfAttrThFifo | kEvfAttrShared)) {
    return ErrorCode::INVAL;
  }

  switch (attrs & (kEvfAttrSingle | kEvfAttrMulti)) {
  case 0:
  case kEvfAttrSingle | kEvfAttrMulti:
    attrs = (attrs & ~(kEvfAttrSingle | kEvfAttrMulti)) | kEvfAttrSingle;
    break;

  default:
    break;
  }

  switch (attrs & (kEvfAttrThPrio | kEvfAttrThFifo)) {
  case 0:
  case kEvfAttrThPrio | kEvfAttrThFifo:
    attrs = (attrs & ~(kEvfAttrThPrio | kEvfAttrThFifo)) | kEvfAttrThFifo;
    break;

  default:
    break;
  }

  char _name[32];
  if (auto result = ureadString(_name, sizeof(_name), (const char *)name);
      result != ErrorCode{}) {
    return result;
  }

  EventFlag *eventFlag;
  if (attrs & kEvfAttrShared) {
    auto [insertedEvf, inserted] =
        thread->tproc->context->createEventFlag(_name, attrs);

    if (!inserted) {
      return ErrorCode::EXIST; // FIXME: verify
    }

    eventFlag = insertedEvf;
  } else {
    eventFlag = knew<EventFlag>(attrs);
  }

  thread->retval[0] = thread->tproc->evfMap.insert(eventFlag);
  return {};
}
orbis::SysResult orbis::sys_evf_delete(Thread *thread, sint id) {
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);
  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  thread->tproc->evfMap.destroy(id);
  return {};
}
orbis::SysResult orbis::sys_evf_open(Thread *thread, ptr<const char[32]> name) {
  ORBIS_LOG_WARNING(__FUNCTION__, name);
  char _name[32];
  if (auto result = ureadString(_name, sizeof(_name), (const char *)name);
      result != ErrorCode{}) {
    return result;
  }

  auto eventFlag = thread->tproc->context->findEventFlag(_name);

  if (eventFlag == nullptr) {
    // HACK :)
    return sys_evf_create(thread, name, kEvfAttrShared, nullptr);
    return ErrorCode::SRCH;
  }

  std::strcpy(eventFlag->name, _name);
  thread->retval[0] = thread->tproc->evfMap.insert(eventFlag);
  return {};
}
orbis::SysResult orbis::sys_evf_close(Thread *thread, sint id) {
  if (!thread->tproc->evfMap.close(id)) {
    return ErrorCode::SRCH;
  }

  return {};
}
orbis::SysResult orbis::sys_evf_wait(Thread *thread, sint id,
                                     uint64_t patternSet, uint64_t mode,
                                     ptr<uint64_t> pPatternSet,
                                     ptr<uint> pTimeout) {
  if ((mode & (kEvfWaitModeAnd | kEvfWaitModeOr)) == 0 ||
      (mode & ~(kEvfWaitModeAnd | kEvfWaitModeOr | kEvfWaitModeClearAll |
                kEvfWaitModeClearPat)) != 0 ||
      patternSet == 0) {
    return ErrorCode::INVAL;
  }

  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  std::uint32_t resultTimeout{};
  std::uint64_t resultPattern{};
  auto result = evf->wait(thread, mode, patternSet,
                          pPatternSet != nullptr ? &resultPattern : nullptr,
                          pTimeout != nullptr ? &resultTimeout : nullptr);

  if (pPatternSet != nullptr) {
    uwrite(pPatternSet, (uint64_t)resultPattern);
  }

  if (pTimeout != nullptr) {
    uwrite(pTimeout, (uint32_t)resultTimeout);
  }
  return result;
}

orbis::SysResult orbis::sys_evf_trywait(Thread *thread, sint id,
                                        uint64_t patternSet, uint64_t mode,
                                        ptr<uint64_t> pPatternSet) {
  if ((mode & (kEvfWaitModeAnd | kEvfWaitModeOr)) == 0 ||
      (mode & ~(kEvfWaitModeAnd | kEvfWaitModeOr | kEvfWaitModeClearAll |
                kEvfWaitModeClearPat)) != 0 ||
      patternSet == 0) {
    return ErrorCode::INVAL;
  }

  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  std::uint64_t resultPattern{};
  auto result = evf->tryWait(thread, mode, patternSet,
                             pPatternSet != nullptr ? &resultPattern : nullptr);

  if (pPatternSet != nullptr) {
    uwrite(pPatternSet, (uint64_t)resultPattern);
  }

  return result;
}
orbis::SysResult orbis::sys_evf_set(Thread *thread, sint id, uint64_t value) {
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  evf->set(value);
  return {};
}
orbis::SysResult orbis::sys_evf_clear(Thread *thread, sint id, uint64_t value) {
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  evf->clear(value);
  return {};
}
orbis::SysResult orbis::sys_evf_cancel(Thread *thread, sint id, uint64_t value,
                                       ptr<sint> pNumWaitThreads) {
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  auto numWaitThreads = evf->cancel(value);
  if (pNumWaitThreads != nullptr) {
    *pNumWaitThreads = numWaitThreads;
  }
  return {};
}
orbis::SysResult orbis::sys_query_memory_protection(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_batch_map(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_create(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_delete(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_open(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_close(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_wait(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_trywait(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_post(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_cancel(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_namedobj_create(Thread *thread,
                                            ptr<const char[32]> name,
                                            ptr<void> object, uint16_t type) {
  ORBIS_LOG_NOTICE(__FUNCTION__, *name, object, type);
  char _name[32];
  if (auto result = ureadString(_name, sizeof(_name), (const char *)name);
      result != ErrorCode{}) {
    return result;
  }
  if (type < 0x101 || type > 0x104) {
    if (type != 0x107)
      ORBIS_LOG_ERROR(__FUNCTION__, *name, object, type);
  }

  std::lock_guard lock(thread->tproc->namedObjMutex);
  auto [id, obj] = thread->tproc->namedObjIds.emplace(object, type);
  if (!obj) {
    return ErrorCode::AGAIN;
  }
  if (!thread->tproc->namedObjNames.try_emplace(object, _name).second) {
    ORBIS_LOG_ERROR("Named object: pointer colflict", type, object);
  }

  return {};
}
orbis::SysResult orbis::sys_namedobj_delete(Thread *thread, uint16_t id,
                                            uint16_t type) {
  ORBIS_LOG_NOTICE(__FUNCTION__, id, type);
  if (id == 0)
    return ErrorCode::INVAL;
  if (type < 0x101 || type > 0x104) {
    if (type != 0x107)
      ORBIS_LOG_ERROR(__FUNCTION__, id, type);
  }

  std::lock_guard lock(thread->tproc->namedObjMutex);
  if (!thread->tproc->namedObjIds.get(id))
    return ErrorCode::SRCH;
  auto [object, ty] = *thread->tproc->namedObjIds.get(id);
  if (ty != type) {
    ORBIS_LOG_ERROR("Named object: found with incorrect type", ty, type);
  }

  if (!thread->tproc->namedObjNames.erase(object)) {
    ORBIS_LOG_ERROR("Named object: pointer not found", type, object);
  }

  return {};
}
orbis::SysResult orbis::sys_set_vm_container(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_debug_init(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_suspend_process(Thread *thread, pid_t pid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_resume_process(Thread *thread, pid_t pid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_opmc_enable(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_opmc_disable(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_opmc_set_ctl(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_opmc_set_ctr(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_opmc_get_ctr(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_budget_create(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_budget_delete(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_budget_get(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_budget_set(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_virtual_query(Thread *thread, ptr<void> addr,
                                          uint64_t unk, ptr<void> info,
                                          size_t infosz) {
  if (auto virtual_query = thread->tproc->ops->virtual_query) {
    return virtual_query(thread, addr, unk, info, infosz);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mdbg_call(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_sblock_create(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_sblock_delete(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_sblock_enter(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_sblock_exit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_sblock_xenter(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_sblock_xexit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_eport_create(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_eport_delete(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_eport_trigger(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_eport_open(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_obs_eport_close(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_is_in_sandbox(Thread *thread /* TODO */) {
  std::printf("sys_is_in_sandbox() -> 0\n");
  return {};
}
orbis::SysResult orbis::sys_dmem_container(Thread *thread) {
  thread->retval[0] = 0; // returns default direct memory device
  return {};
}
orbis::SysResult orbis::sys_get_authinfo(Thread *thread, pid_t pid,
                                         ptr<void> info) {
  struct authinfo {
    uint64_t a;
    uint64_t b;
  };

  std::memset(info, 0, 136);
  ((authinfo *)info)->b = ~0;

  return {};
}
orbis::SysResult orbis::sys_mname(Thread *thread, ptr<void> address,
                                  uint64_t length, ptr<const char> name) {
  std::printf("sys_mname(%p, %p, '%s')\n", address, (char *)address + length,
              name);
  return {};
}
orbis::SysResult orbis::sys_dynlib_dlopen(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_dlclose(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_dlsym(Thread *thread, SceKernelModule handle,
                                         ptr<const char> symbol,
                                         ptr<ptr<void>> addrp) {
  if (thread->tproc->ops->dynlib_dlsym) {
    return thread->tproc->ops->dynlib_dlsym(thread, handle, symbol, addrp);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_get_list(Thread *thread,
                                            ptr<SceKernelModule> pArray,
                                            size_t numArray,
                                            ptr<size_t> pActualNum) {
  std::size_t actualNum = 0;
  for (auto [id, module] : thread->tproc->modulesMap) {
    if (actualNum >= numArray) {
      break;
    }

    pArray[actualNum++] = id;
  }
  uwrite(pActualNum, actualNum);
  return {};
}
orbis::SysResult orbis::sys_dynlib_get_info(Thread *thread,
                                            SceKernelModule handle,
                                            ptr<ModuleInfo> pInfo) {
  auto module = thread->tproc->modulesMap.get(handle);

  if (module == nullptr) {
    return ErrorCode::SRCH;
  }

  if (pInfo->size != sizeof(ModuleInfo)) {
    return ErrorCode::INVAL;
  }

  ModuleInfo result = {};
  result.size = sizeof(ModuleInfo);
  std::strncpy(result.name, module->moduleName, sizeof(result.name));
  std::memcpy(result.segments, module->segments,
              sizeof(ModuleSegment) * module->segmentCount);
  result.segmentCount = module->segmentCount;
  std::memcpy(result.fingerprint, module->fingerprint,
              sizeof(result.fingerprint));
  uwrite(pInfo, result);
  return {};
}
orbis::SysResult orbis::sys_dynlib_load_prx(Thread *thread,
                                            ptr<const char> name, uint64_t arg1,
                                            ptr<ModuleHandle> pHandle,
                                            uint64_t arg3) {
  if (auto dynlib_load_prx = thread->tproc->ops->dynlib_load_prx) {
    return dynlib_load_prx(thread, name, arg1, pHandle, arg3);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_unload_prx(Thread *thread,
                                              SceKernelModule handle) {
  if (auto dynlib_unload_prx = thread->tproc->ops->dynlib_unload_prx) {
    return dynlib_unload_prx(thread, handle);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_do_copy_relocations(Thread *thread) {
  if (auto dynlib_do_copy_relocations =
          thread->tproc->ops->dynlib_do_copy_relocations) {
    return dynlib_do_copy_relocations(thread);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_prepare_dlclose(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_dynlib_get_proc_param(Thread *thread,
                                                  ptr<ptr<void>> procParam,
                                                  ptr<uint64_t> procParamSize) {
  auto proc = thread->tproc;
  *procParam = proc->processParam;
  *procParamSize = proc->processParamSize;
  return {};
}
orbis::SysResult orbis::sys_dynlib_process_needed_and_relocate(Thread *thread) {
  if (auto processNeeded = thread->tproc->ops->processNeeded) {
    auto result = processNeeded(thread);

    if (result.value() != 0) {
      return result;
    }
  }

  for (auto [id, module] : thread->tproc->modulesMap) {
    auto result = module->relocate(thread->tproc);
    if (result.isError()) {
      return result;
    }
  }

  if (auto registerEhFrames = thread->tproc->ops->registerEhFrames) {
    auto result = registerEhFrames(thread);

    if (result.value() != 0) {
      return result;
    }
  }

  return {};
}
orbis::SysResult orbis::sys_sandbox_path(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}

struct mdbg_property {
  orbis::int32_t unk;
  orbis::int32_t unk2;
  orbis::uint64_t addr_ptr;
  orbis::uint64_t areaSize;
  orbis::int64_t unk3;
  orbis::int64_t unk4;
  char name[32];
};

orbis::SysResult orbis::sys_mdbg_service(Thread *thread, uint32_t op,
                                         ptr<void> arg0, ptr<void> arg1) {
  std::printf("sys_mdbg_service(op = %d, arg0 = %p, arg1 = %p)\n", op, arg0,
              arg1);

  switch (op) {
  case 1: {
    auto prop = uread((ptr<mdbg_property>)arg0);
    ORBIS_LOG_WARNING(__FUNCTION__, prop.name, prop.addr_ptr, prop.areaSize);
    break;
  }

  case 3: {
    auto errorCode = (unsigned)(uint64_t)(arg0);
    ORBIS_LOG_ERROR("sys_mdbg_service: ERROR CODE", errorCode);
    break;
  }

  case 7: {
    // TODO: read string from userspace
    std::printf("sys_mdbg_service: %s\n", (char *)arg0);
    break;
  }

  default:
    break;
  }

  return {};
}
orbis::SysResult orbis::sys_randomized_path(Thread *thread /* TODO */) {
  std::printf("TODO: sys_randomized_path()\n");
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_rdup(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dl_get_metadata(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_workaround8849(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_is_development_mode(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_self_auth_info(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_dynlib_get_info_ex(Thread *thread, SceKernelModule handle,
                              ptr<struct Unk> unk,
                              ptr<ModuleInfoEx> destModuleInfoEx) {
  auto module = thread->tproc->modulesMap.get(handle);
  if (module == nullptr) {
    return ErrorCode::SRCH;
  }

  if (destModuleInfoEx->size != sizeof(ModuleInfoEx)) {
    return ErrorCode::INVAL;
  }

  ModuleInfoEx result = {};
  result.size = sizeof(ModuleInfoEx);
  std::strncpy(result.name, module->moduleName, sizeof(result.name));
  result.id = std::to_underlying(handle);
  result.tlsIndex = module->tlsIndex;
  result.tlsInit = module->tlsInit;
  result.tlsInitSize = module->tlsInitSize;
  result.tlsSize = module->tlsSize;
  result.tlsOffset = module->tlsOffset;
  result.tlsAlign = module->tlsAlign;
  result.initProc = module->initProc;
  result.finiProc = module->finiProc;
  result.ehFrameHdr = module->ehFrameHdr;
  result.ehFrame = module->ehFrame;
  result.ehFrameHdrSize = module->ehFrameHdrSize;
  result.ehFrameSize = module->ehFrameSize;
  std::memcpy(result.segments, module->segments,
              sizeof(ModuleSegment) * module->segmentCount);
  result.segmentCount = module->segmentCount;
  result.refCount = module->references.load(std::memory_order::relaxed);
  uwrite(destModuleInfoEx, result);

  return {};
}
orbis::SysResult orbis::sys_budget_getid(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_budget_get_ptype(Thread *thread, sint budgetId) {
  thread->retval[0] = 1;
  return {};
}
orbis::SysResult
orbis::sys_get_paging_stats_of_all_threads(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_proc_type_info(Thread *thread,
                                               ptr<sint> destProcessInfo) {
  std::printf("TODO: sys_get_proc_type_info\n");

  struct dargs {
    uint64_t size = sizeof(dargs);
    uint32_t ptype;
    uint32_t pflags;
  } args = {.ptype = 1, .pflags = 0};

  uwrite((ptr<dargs>)destProcessInfo, args);
  return {};
}
orbis::SysResult orbis::sys_get_resident_count(Thread *thread, pid_t pid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_prepare_to_suspend_process(Thread *thread,
                                                       pid_t pid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_resident_fmem_count(Thread *thread, pid_t pid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_thr_get_name(Thread *thread, lwpid_t lwpid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_set_gpo(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_get_paging_stats_of_all_objects(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_test_debug_rwmem(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_free_stack(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_suspend_system(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}

enum ImpiOpcode { kIpmiCreateClient = 2 };

struct IpmiCreateClientParams {
  orbis::ptr<void> arg0;
  orbis::ptr<char> name;
  orbis::ptr<void> arg2;
};

static_assert(sizeof(IpmiCreateClientParams) == 0x18);

orbis::SysResult orbis::sys_ipmimgr_call(Thread *thread, uint64_t id,
                                         uint64_t arg2, ptr<uint64_t> result,
                                         ptr<uint64_t> params,
                                         uint64_t paramsSize, uint64_t arg6) {
  std::printf("TODO: sys_ipmimgr_call(id = %lld)\n", (unsigned long long)id);

  if (id == kIpmiCreateClient) {
    if (paramsSize != sizeof(IpmiCreateClientParams)) {
      return ErrorCode::INVAL;
    }

    auto createParams = (ptr<IpmiCreateClientParams>)params;

    std::printf("ipmi create client(%p, '%s', %p)\n",
                (void *)createParams->arg0, (char *)createParams->name,
                (void *)createParams->arg2);

    return {};
  }

  if (id == 1131 || id == 1024 || id == 800) {
    thread->retval[0] = -0x40004; // HACK
    return {};
    // return -0x40004;
  }

  if (id == 3) {
    if (result) {
      *result = 0;
    }
    return {};
  }

  return {};
}
orbis::SysResult orbis::sys_get_gpo(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_vm_map_timestamp(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_opmc_set_hw(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_opmc_get_hw(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_cpu_usage_all(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mmap_dmem(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_physhm_open(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_physhm_unlink(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_resume_internal_hdd(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_thr_suspend_ucontext(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_thr_resume_ucontext(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_thr_get_ucontext(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_thr_set_ucontext(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_set_timezone_info(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_set_phys_fmem_limit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_utc_to_localtime(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_localtime_to_utc(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_set_uevt(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_cpu_usage_proc(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_map_statistics(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_set_chicken_switches(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_extend_page_table_pool(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_extend_page_table_pool2(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_get_kernel_mem_statistics(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_get_sdk_compiled_version(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_app_state_change(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_get_obj_member(Thread *thread,
                                                  SceKernelModule handle,
                                                  uint64_t index,
                                                  ptr<ptr<void>> addrp) {
  if (auto dynlib_get_obj_member = thread->tproc->ops->dynlib_get_obj_member) {
    return dynlib_get_obj_member(thread, handle, index, addrp);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_budget_get_ptype_of_budget(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_prepare_to_resume_process(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_process_terminate(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_open(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_map(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_unmap(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_dynlib_get_info_for_libdbg(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_batch(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fdatasync(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_get_list2(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dynlib_get_info2(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_submit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_multi_delete(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_multi_wait(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_multi_poll(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_get_data(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_multi_cancel(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_bio_usage_all(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_create(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_submit_cmd(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_init(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_page_table_stats(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_dynlib_get_list_for_libdbg(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_move(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_virtual_query_all(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_reserve_2mb_page(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cpumode_yield(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_wait6(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_rights_limit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_ioctls_limit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_ioctls_get(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_fcntls_limit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_cap_fcntls_get(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_bindat(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_connectat(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_chflagsat(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_accept4(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_pipe2(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_aio_mlock(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_procctl(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ppoll(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_futimens(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_utimensat(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_numa_getaffinity(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_numa_setaffinity(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_apr_submit(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_apr_resolve(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_apr_stat(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_apr_wait(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_apr_ctrl(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_phys_page_size(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_begin_app_mount(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_end_app_mount(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_fsc2h_ctrl(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_streamwrite(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_app_save(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_app_restore(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_saved_app_delete(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_get_ppr_sdk_compiled_version(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_notify_app_event(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ioreq(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_openintr(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_dl_get_info_2(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_acinfo_add(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_acinfo_delete(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_acinfo_get_all_for_coredump(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ampr_ctrl_debug(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_workspace_ctrl(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
