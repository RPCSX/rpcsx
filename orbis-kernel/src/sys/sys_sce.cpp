#include "sys/sys_sce.hpp"
#include "KernelContext.hpp"
#include "error.hpp"
#include "evf.hpp"
#include "ipmi.hpp"
#include "module/ModuleInfo.hpp"
#include "module/ModuleInfoEx.hpp"
#include "orbis/time.hpp"
#include "osem.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

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
  ORBIS_LOG_TODO(__FUNCTION__, name, domain, type, protocol);
  if (auto socket = thread->tproc->ops->socket) {
    Ref<File> file;
    auto result = socket(thread, name, domain, type, protocol, &file);

    if (result.isError()) {
      return result;
    }

    auto fd = thread->tproc->fileDescriptors.insert(file);
    ORBIS_LOG_WARNING("Socket opened", name, fd);
    thread->retval[0] = fd;
    return {};
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_socketclose(Thread *thread, sint fd) {
  // This syscall is identical to sys_close
  ORBIS_LOG_NOTICE(__FUNCTION__, fd);
  if (thread->tproc->fileDescriptors.close(fd)) {
    return {};
  }

  return ErrorCode::BADF;
}
orbis::SysResult orbis::sys_netgetiflist(Thread *thread /* TODO */) {
  return {};
}

orbis::SysResult orbis::sys_mtypeprotect(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_regmgr_call(Thread *thread, uint32_t op,
                                        uint32_t id, ptr<void> result,
                                        ptr<void> value, uint64_t len) {
  // ORBIS_LOG_ERROR(__FUNCTION__, op, id, len);
  // thread->where();

  if (op == 2) {
    // get int
    if (len != sizeof(uint32_t)) {
      return ErrorCode::INVAL;
    }

    if (id == 0x2010000) {
      return uwrite((ptr<uint>)value, 0u);
    }
    if (id == 0x7802c000) {
      return uwrite((ptr<uint>)value, 0u);
    }
    if (id == 0x78020500) {
      return uwrite((ptr<uint>)value, 0u);
    }
    if (id == 0x78020b00) {
      return uwrite((ptr<uint>)value, 0u);
    }

    return {};
  }

  if (op == 4) {
    // get string

    if (id == 0x7802e601) {
      // std::strcpy((ptr<char>)value, "Application/app.exe.self");
      return ErrorCode::INVAL;
    }

    ORBIS_LOG_ERROR(__FUNCTION__, op, id, len);
    thread->where();
    return {};
  }

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

    // HACK: set default system language and gamepad layout to US/EU region
    // 0x12356328ECF5617B -> language where is 0 is Japanese, 1 is English
    // 0x22666251FE7BECFF -> confirm button layout, 0 is Circle, 1 is Cross
    if (int_value->encoded_id == 0x12356328ECF5617B ||
        int_value->encoded_id == 0x22666251FE7BECFF) {
      int_value->value = 1;
      return {};
    }

    if (int_value->encoded_id == 0x1ac46343411b3f40) {
      int_value->value = 0; // allow debug libraries
      return {};
    }

    // 0x503f69bde385a6ac // allow loading from dev machine?
    // 0x2d946f62aef8f878

    int_value->value = 0;
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
                                       sint attrs, uint64_t initPattern) {
  ORBIS_LOG_WARNING(__FUNCTION__, name, attrs, initPattern);
  if (name == nullptr) {
    return ErrorCode::INVAL;
  }

  if (attrs & ~(kEvfAttrSingle | kEvfAttrMulti | kEvfAttrThPrio |
                kEvfAttrThFifo | kEvfAttrShared)) {
    return ErrorCode::INVAL;
  }

  switch (attrs & (kEvfAttrSingle | kEvfAttrMulti)) {
  case kEvfAttrSingle | kEvfAttrMulti:
    return ErrorCode::INVAL;
  case 0:
    attrs |= kEvfAttrSingle;
    break;

  default:
    break;
  }

  switch (attrs & (kEvfAttrThPrio | kEvfAttrThFifo)) {
  case kEvfAttrThPrio | kEvfAttrThFifo:
    return ErrorCode::INVAL;
  case 0:
    attrs |= kEvfAttrThFifo;
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
        thread->tproc->context->createEventFlag(_name, attrs, initPattern);

    if (!inserted) {
      return ErrorCode::EXIST; // FIXME: verify
    }

    eventFlag = insertedEvf;
  } else {
    eventFlag = knew<EventFlag>(attrs, initPattern);
    std::strncpy(eventFlag->name, _name, 32);
  }

  thread->retval[0] = thread->tproc->evfMap.insert(eventFlag);
  return {};
}
orbis::SysResult orbis::sys_evf_delete(Thread *thread, sint id) {
  ORBIS_LOG_WARNING(__FUNCTION__, id);
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);
  if (evf == nullptr) {
    return ErrorCode::NOENT;
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
    return ErrorCode::NOENT;
  }

  thread->retval[0] = thread->tproc->evfMap.insert(eventFlag);
  return {};
}
orbis::SysResult orbis::sys_evf_close(Thread *thread, sint id) {
  ORBIS_LOG_WARNING(__FUNCTION__, thread->tid, id);
  if (!thread->tproc->evfMap.close(id)) {
    return ErrorCode::NOENT;
  }

  return {};
}
orbis::SysResult orbis::sys_evf_wait(Thread *thread, sint id,
                                     uint64_t patternSet, uint64_t mode,
                                     ptr<uint64_t> pPatternSet,
                                     ptr<uint> pTimeout) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread->tid, id, patternSet, mode, pPatternSet,
                  pTimeout);
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
  auto result = evf->wait(thread, mode, patternSet,
                          pTimeout != nullptr ? &resultTimeout : nullptr);

  ORBIS_LOG_TRACE("sys_evf_wait wakeup", thread->tid, thread->evfResultPattern);

  if (pPatternSet != nullptr) {
    ORBIS_RET_ON_ERROR(uwrite(pPatternSet, thread->evfResultPattern));
  }

  if (pTimeout != nullptr) {
    ORBIS_RET_ON_ERROR(uwrite(pTimeout, resultTimeout));
  }

  if (result == ErrorCode::TIMEDOUT)
    return SysResult::notAnError(result);

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

  auto result = evf->tryWait(thread, mode, patternSet);
  ORBIS_LOG_TRACE(__FUNCTION__, evf->name, thread->tid, id, patternSet, mode,
                  pPatternSet, result);

  if (pPatternSet != nullptr) {
    ORBIS_RET_ON_ERROR(uwrite(pPatternSet, thread->evfResultPattern));
  }

  if (result == ErrorCode::BUSY) {
    return SysResult::notAnError(result);
  }
  return result;
}
orbis::SysResult orbis::sys_evf_set(Thread *thread, sint id, uint64_t value) {
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  ORBIS_LOG_TRACE(__FUNCTION__, evf->name, thread->tid, id, value);
  evf->set(value);
  return {};
}
orbis::SysResult orbis::sys_evf_clear(Thread *thread, sint id, uint64_t value) {
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  ORBIS_LOG_TRACE(__FUNCTION__, evf->name, thread->tid, id, value);
  evf->clear(value);
  return {};
}
orbis::SysResult orbis::sys_evf_cancel(Thread *thread, sint id, uint64_t value,
                                       ptr<sint> pNumWaitThreads) {
  Ref<EventFlag> evf = thread->tproc->evfMap.get(id);

  if (evf == nullptr) {
    return ErrorCode::SRCH;
  }

  ORBIS_LOG_TRACE(__FUNCTION__, evf->name, thread->tid, id, value,
                  pNumWaitThreads);

  auto numWaitThreads = evf->cancel(value);
  if (pNumWaitThreads != 0) {
    return uwrite(pNumWaitThreads, static_cast<sint>(numWaitThreads));
  }
  return {};
}
orbis::SysResult
orbis::sys_query_memory_protection(Thread *thread, ptr<void> address,
                                   ptr<MemoryProtection> protection) {
  if (auto query_memory_protection =
          thread->tproc->ops->query_memory_protection) {
    return query_memory_protection(thread, address, protection);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_batch_map(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_osem_create(Thread *thread,
                                        ptr<const char[32]> name, uint attrs,
                                        sint initCount, sint maxCount) {
  ORBIS_LOG_WARNING(__FUNCTION__, name, attrs, initCount, maxCount);
  if (name == nullptr) {
    return ErrorCode::INVAL;
  }

  if (attrs & ~(kSemaAttrThPrio | kSemaAttrThFifo | kSemaAttrShared)) {
    return ErrorCode::INVAL;
  }

  switch (attrs & (kSemaAttrThPrio | kSemaAttrThFifo)) {
  case kSemaAttrThPrio | kSemaAttrThFifo:
    return ErrorCode::INVAL;
  case 0:
    attrs |= kSemaAttrThFifo;
    break;

  default:
    break;
  }

  if (maxCount <= 0 || initCount < 0 || maxCount < initCount)
    return ErrorCode::INVAL;

  char _name[32];
  if (auto result = ureadString(_name, sizeof(_name), (const char *)name);
      result != ErrorCode{}) {
    return result;
  }

  Semaphore *sem;
  if (attrs & kSemaAttrShared) {
    auto [insertedSem, ok] = thread->tproc->context->createSemaphore(
        _name, attrs, initCount, maxCount);

    if (!ok) {
      return ErrorCode::EXIST; // FIXME: verify
    }

    std::strncpy(insertedSem->name, _name, 32);
    sem = insertedSem;
  } else {
    sem = knew<Semaphore>(attrs, initCount, maxCount);
  }

  thread->retval[0] = thread->tproc->semMap.insert(sem);
  return {};
}
orbis::SysResult orbis::sys_osem_delete(Thread *thread, sint id) {
  ORBIS_LOG_TRACE(__FUNCTION__, id);
  Ref<Semaphore> sem = thread->tproc->semMap.get(id);
  if (sem == nullptr) {
    return ErrorCode::NOENT;
  }

  thread->tproc->semMap.destroy(id);
  return {};
}
orbis::SysResult orbis::sys_osem_open(Thread *thread,
                                      ptr<const char[32]> name) {
  ORBIS_LOG_WARNING(__FUNCTION__, name);
  char _name[32];
  if (auto result = ureadString(_name, sizeof(_name), (const char *)name);
      result != ErrorCode{}) {
    return result;
  }

  auto sem = thread->tproc->context->findSemaphore(_name);
  if (sem == nullptr) {
    return ErrorCode::NOENT;
  }

  thread->retval[0] = thread->tproc->semMap.insert(sem);
  return {};
}
orbis::SysResult orbis::sys_osem_close(Thread *thread, sint id) {
  ORBIS_LOG_TRACE(__FUNCTION__, id);
  if (!thread->tproc->semMap.close(id)) {
    return ErrorCode::NOENT;
  }

  return {};
}
orbis::SysResult orbis::sys_osem_wait(Thread *thread, sint id, sint need,
                                      ptr<uint> pTimeout) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread, id, need, pTimeout);
  Ref<Semaphore> sem = thread->tproc->semMap.get(id);
  if (need < 1 || need > sem->maxValue)
    return ErrorCode::INVAL;

  auto deadline = std::chrono::steady_clock::time_point::max();
  if (pTimeout != nullptr) {
    deadline =
        std::chrono::steady_clock::now() + std::chrono::microseconds(*pTimeout);
  }

  std::lock_guard lock(sem->mtx);
  bool timedout = false;
  while (true) {
    if (sem->isDeleted)
      return ErrorCode::ACCES;
    if (sem->value >= need) {
      sem->value -= need;
      break;
    }
    long ut = -1;
    if (deadline != std::chrono::steady_clock::time_point::max()) {
      ut = std::chrono::duration_cast<std::chrono::microseconds>(
               deadline - std::chrono::steady_clock::now())
               .count();
      if (ut < 0) {
        timedout = true;
        break;
      }
    }
    sem->cond.wait(sem->mtx, ut);
  }

  if (pTimeout) {
    if (timedout) {
      *pTimeout = 0;
    } else {
      auto timeout = std::chrono::duration_cast<std::chrono::microseconds>(
                         deadline - std::chrono::steady_clock::now())
                         .count();

      if (timeout > 0) {
        *pTimeout = timeout;
      } else {
        *pTimeout = 0;
      }
    }
  }

  if (timedout) {
    return SysResult::notAnError(ErrorCode::TIMEDOUT);
  }
  return {};
}
orbis::SysResult orbis::sys_osem_trywait(Thread *thread, sint id, sint need) {
  ORBIS_LOG_TRACE(__FUNCTION__, thread, id, need);
  Ref<Semaphore> sem = thread->tproc->semMap.get(id);
  if (need < 1 || need > sem->maxValue)
    return ErrorCode::INVAL;

  std::lock_guard lock(sem->mtx);
  if (sem->isDeleted || sem->value < need)
    return orbis::SysResult::notAnError(ErrorCode::BUSY);
  sem->value -= need;
  return {};
}
orbis::SysResult orbis::sys_osem_post(Thread *thread, sint id, sint count) {
  ORBIS_LOG_WARNING(__FUNCTION__, thread, id, count);
  Ref<Semaphore> sem = thread->tproc->semMap.get(id);
  if (count < 1 || count > sem->maxValue - sem->value)
    return ErrorCode::INVAL;

  std::lock_guard lock(sem->mtx);
  if (sem->isDeleted)
    return {};
  sem->value += count;
  sem->cond.notify_all(sem->mtx);
  return {};
}
orbis::SysResult orbis::sys_osem_cancel(Thread *thread, sint id, sint set,
                                        ptr<uint> pNumWaitThreads) {
  ORBIS_LOG_TODO(__FUNCTION__, thread, id, set, pNumWaitThreads);
  std::abort();
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_namedobj_create(Thread *thread,
                                            ptr<const char[32]> name,
                                            ptr<void> object, uint16_t type) {
  ORBIS_LOG_NOTICE(__FUNCTION__, name, object, type);
  if (!name)
    return ErrorCode::INVAL;
  char _name[32];
  if (auto result = ureadString(_name, sizeof(_name), (const char *)name);
      result != ErrorCode{}) {
    return result;
  }
  if (type < 0x101 || type > 0x104) {
    if (type != 0x107)
      ORBIS_LOG_ERROR(__FUNCTION__, name, object, type);
  }

  std::lock_guard lock(thread->tproc->namedObjMutex);
  auto [id, obj] = thread->tproc->namedObjIds.emplace(object, type);
  if (!obj) {
    return ErrorCode::AGAIN;
  }
  if (!thread->tproc->namedObjNames.try_emplace(object, _name).second) {
    ORBIS_LOG_ERROR("Named object: pointer colflict", type, object);
  }

  thread->retval[0] = id;
  return {};
}
orbis::SysResult orbis::sys_namedobj_delete(Thread *thread, uint id,
                                            uint16_t type) {
  ORBIS_LOG_NOTICE(__FUNCTION__, id, type);
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

  thread->tproc->namedObjIds.destroy(id);
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
orbis::SysResult orbis::sys_budget_get(Thread *thread, sint id, ptr<void> a,
                                       ptr<uint32_t> count) {
  return {};
}
orbis::SysResult orbis::sys_budget_set(Thread *thread, slong budget) {
  ORBIS_LOG_TODO(__FUNCTION__, budget);
  return {};
}
orbis::SysResult orbis::sys_virtual_query(Thread *thread, ptr<void> addr,
                                          uint64_t unk, ptr<void> info,
                                          size_t infosz) {
  if (auto virtual_query = thread->tproc->ops->virtual_query) {
    return virtual_query(thread, addr, unk, info, infosz);
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_mdbg_call(Thread *thread /* TODO */) { return {}; }
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
  ORBIS_LOG_ERROR(__FUNCTION__, thread->tproc->isInSandbox);
  thread->retval[0] = thread->tproc->isInSandbox ? 1 : 0;
  return {};
}
orbis::SysResult orbis::sys_dmem_container(Thread *thread, uint id) {
  ORBIS_LOG_NOTICE(__FUNCTION__, id);
  thread->retval[0] = 1; // returns default direct memory device
  if (id + 1)
    return ErrorCode::PERM;
  return {};
}
orbis::SysResult orbis::sys_get_authinfo(Thread *thread, pid_t pid,
                                         ptr<AuthInfo> info) {
  return uwrite(info, thread->tproc->authInfo);
}
orbis::SysResult orbis::sys_mname(Thread *thread, uint64_t addr, uint64_t len,
                                  ptr<const char[32]> name) {
  ORBIS_LOG_NOTICE(__FUNCTION__, addr, len, name);
  if (addr < 0x40000 || addr >= 0x100'0000'0000 || 0x100'0000'0000 - addr < len)
    return ErrorCode::INVAL;
  char _name[32];
  if (auto result = ureadString(_name, sizeof(_name), (const char *)name);
      result != ErrorCode{}) {
    return result;
  }

  NamedMemoryRange range;
  range.begin = addr & ~0x3fffull;
  range.end = range.begin;
  range.end += ((addr & 0x3fff) + 0x3fff + len) & ~0x3fffull;

  std::lock_guard lock(thread->tproc->namedMemMutex);
  auto [it, end] = thread->tproc->namedMem.equal_range<NamedMemoryRange>(range);
  while (it != end) {
    auto [addr2, end2] = it->first;
    auto len2 = end2 - addr2;
    ORBIS_LOG_NOTICE("sys_mname: removed overlapped", it->second, addr2, len2);
    it = thread->tproc->namedMem.erase(it);
  }
  if (!thread->tproc->namedMem.try_emplace(range, _name).second)
    std::abort();
  if (!thread->tproc->namedMem.count(addr))
    std::abort();

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
  ORBIS_LOG_WARNING(__FUNCTION__, symbol);
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
  return uwrite(pActualNum, actualNum);
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
  return uwrite(pInfo, result);
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
  ORBIS_LOG_NOTICE(__FUNCTION__);
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

  ORBIS_LOG_WARNING(__FUNCTION__);
  return {};
}
orbis::SysResult orbis::sys_sandbox_path(Thread *thread, ptr<const char> path) {
  ORBIS_LOG_ERROR(__FUNCTION__, path);
  thread->tproc->isInSandbox = true;
  return {};
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
  // ORBIS_LOG_NOTICE("sys_mdbg_service", thread->tid, op, arg0, arg1);
  // thread->where();

  switch (op) {
  case 1: {
    mdbg_property prop;
    if (auto error = uread(prop, (ptr<mdbg_property>)arg0);
        error != ErrorCode{})
      return error;
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
    ORBIS_LOG_NOTICE("sys_mdbg_service", (char *)arg0);
    break;
  }

  case 0x14: {
    std::this_thread::sleep_for(std::chrono::years(1));
    break;
  }

  default:
    break;
  }

  return {};
}
orbis::SysResult orbis::sys_randomized_path(Thread *thread, sint type,
                                            ptr<char> path, ptr<sint> length) {
  ORBIS_LOG_TODO(__FUNCTION__, type, (ptr<void>)path, length ? *length : 0);
  if (type == 0) {
    if (thread->tproc->isInSandbox) {
      std::strcpy(path, "system");
      *length = sizeof("system") - 1;
      return {};
    }

    *path = '\0';
    *length = 0;
  } else {
    thread->where();
  }
  return {};
}
orbis::SysResult orbis::sys_rdup(Thread *thread, sint pid, sint fd) {
  ORBIS_LOG_TODO(__FUNCTION__, pid, fd);
  for (auto it = g_context.getProcessList(); it != nullptr; it = it->next) {
    auto &p = it->object;
    if (p.pid != pid) {
      continue;
    }

    auto file = p.fileDescriptors.get(fd);
    if (file == nullptr) {
      return ErrorCode::BADF;
    }

    thread->retval[0] = thread->tproc->fileDescriptors.insert(std::move(file));
    return {};
  }
  return ErrorCode::SRCH;
}
orbis::SysResult orbis::sys_dl_get_metadata(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_workaround8849(Thread *thread /* TODO */) {
  thread->retval[0] = 1;
  return {};
}
orbis::SysResult orbis::sys_is_development_mode(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_get_self_auth_info(Thread *thread,
                                               ptr<const char> path,
                                               ptr<AuthInfo> result) {
  ORBIS_LOG_ERROR(__FUNCTION__, path, result);
  thread->where();

  return uwrite(
      result, {
                  .unk0 = 0x3100000000000001,
                  .caps =
                      {
                          ~0ull, ~0ull, ~0ull, ~0ull,
                          // 0x2000038000000000,
                          // 0x000000000000FF00,
                          // 0x0000000000000000,
                          // 0x0000000000000000,
                      },
                  .attrs =
                      {
                          0x4000400040000000,
                          0x4000000000000000,
                          0x0080000000000004, // lower byte is application type
                          0xF0000000FFFF4000,
                      },
              });

  return {};
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
  result.refCount = 1;
  ORBIS_LOG_WARNING(__FUNCTION__, result.id, result.name, result.tlsIndex,
                    result.tlsInit, result.tlsInitSize, result.tlsSize,
                    result.tlsOffset, result.tlsAlign, result.initProc,
                    result.finiProc, result.ehFrameHdr, result.ehFrame,
                    result.ehFrameHdrSize, result.ehFrameSize,
                    result.segmentCount, result.refCount);
  return uwrite(destModuleInfoEx, result);
}
orbis::SysResult orbis::sys_budget_getid(Thread *thread) {
  thread->retval[0] = thread->tproc->budgetId;
  return {};
}
orbis::SysResult orbis::sys_budget_get_ptype(Thread *thread, sint budgetId) {
  thread->retval[0] = budgetId;
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

  return uwrite((ptr<dargs>)destProcessInfo, args);
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

orbis::SysResult orbis::sys_ipmimgr_call(Thread *thread, uint op, uint kid,
                                         ptr<uint> result, ptr<void> params,
                                         uint64_t paramsSz) {
  // ORBIS_LOG_TODO(__FUNCTION__, thread->tid, op, kid, result, params,
  // paramsSz);

  switch (op) {
  case 0:
    return sysIpmiCreateServer(thread, result, params, paramsSz);
  case 1:
    return sysIpmiDestroyServer(thread, result, kid, params, paramsSz);
  case 2:
    return sysIpmiCreateClient(thread, result, params, paramsSz);
  case 3:
    return sysIpmiDestroyClient(thread, result, kid, params, paramsSz);
  case 4:
    return sysIpmiCreateSession(thread, result, params, paramsSz);
  case 5:
    return sysIpmiDestroySession(thread, result, kid, params, paramsSz);
  case 0x10: // trace
    return uwrite(result, 0u);
  case 0x201:
    return sysIpmiServerReceivePacket(thread, result, kid, params, paramsSz);
  case 0x212:
    return sysIpmiSendConnectResult(thread, result, kid, params, paramsSz);
  case 0x232:
    return sysIpmiSessionRespondSync(thread, result, kid, params, paramsSz);
  case 0x252:
    // TODO: try get message
    return uwrite<uint>(result, 0x80020023);
  case 0x302:
    return sysIpmiSessionGetClientPid(thread, result, kid, params, paramsSz);
  case 0x320:
    return sysIpmiClientInvokeSyncMethod(thread, result, kid, params, paramsSz);
  case 0x400:
    return sysIpmiClientConnect(thread, result, kid, params, paramsSz);
  case 0x468:
    return sysIpmiSessionGetUserData(thread, result, kid, params, paramsSz);
  case 0x46a:
    return sysIpmiServerGetName(thread, result, kid, params, paramsSz);
  case 0x491:
    return sysIpmiClientPollEventFlag(thread, result, kid, params, paramsSz);
  case 0x493:
    return sysIpmiSessionSetEventFlag(thread, result, kid, params, paramsSz);
  }

  ORBIS_LOG_TODO(__FUNCTION__, thread->tid, op, kid, result, params, paramsSz);
  thread->where();
  return uwrite(result, 0u);
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
orbis::SysResult orbis::sys_get_cpu_usage_all(Thread *thread, uint32_t unk,
                                              ptr<uint32_t> result) {
  ORBIS_LOG_TODO(__FUNCTION__, unk, result);
  return {};
}
orbis::SysResult orbis::sys_mmap_dmem(Thread *thread, caddr_t addr, size_t len,
                                      sint memoryType, sint prot, sint flags,
                                      off_t directMemoryStart) {
  if (auto dmem_mmap = thread->tproc->ops->dmem_mmap) {
    return dmem_mmap(thread, addr, len, memoryType, prot, flags,
                     directMemoryStart);
  }
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
orbis::SysResult orbis::sys_utc_to_localtime(Thread *thread, int64_t time,
                                             int64_t *localtime,
                                             orbis::timesec *_sec,
                                             int *_dst_sec) {
  ORBIS_LOG_TRACE(__FUNCTION__, time, localtime, _sec, _dst_sec);
  // Disabled for now
  return SysResult::notAnError(ErrorCode::RANGE);

  struct ::tm tp;
  auto result = ::mktime(::localtime_r(&time, &tp));
  if (auto e = uwrite(localtime, result); e != ErrorCode{})
    return e;
  ORBIS_RET_ON_ERROR(uwrite(_sec, {}));
  ORBIS_RET_ON_ERROR(uwrite(_dst_sec, 0));
  return {};
}
orbis::SysResult orbis::sys_localtime_to_utc(Thread *thread, int64_t time,
                                             uint unk, int64_t *ptime,
                                             orbis::timesec *_sec,
                                             int *_dst_sec) {
  ORBIS_LOG_TRACE(__FUNCTION__, time, unk, ptime, _sec, _dst_sec);
  // Disabled for now
  return SysResult::notAnError(ErrorCode::RANGE);

  struct ::tm tp;
  ::time_t timez = 0;
  auto result = time - ::mktime(::localtime_r(&timez, &tp));
  if (auto e = uwrite(ptime, result); e != ErrorCode{})
    return e;
  ORBIS_RET_ON_ERROR(uwrite(_sec, {}));
  ORBIS_RET_ON_ERROR(uwrite(_dst_sec, 0));
  return {};
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
orbis::sys_get_sdk_compiled_version(Thread *thread, ptr<const char> path) {
  ORBIS_LOG_ERROR(__FUNCTION__, path);
  thread->retval[0] = g_context.sdkVersion;
  return {};
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
orbis::sys_budget_get_ptype_of_budget(Thread *thread, sint budgetId) {
  ORBIS_LOG_WARNING(__FUNCTION__, budgetId);
  thread->retval[0] = budgetId;
  return {};
}
orbis::SysResult
orbis::sys_prepare_to_resume_process(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_process_terminate(Thread *thread /* TODO */) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_open(Thread *thread) {
  if (auto blockpool_open = thread->tproc->ops->blockpool_open) {
    Ref<File> file;
    auto result = blockpool_open(thread, &file);
    if (result.isError()) {
      return result;
    }

    thread->retval[0] = thread->tproc->fileDescriptors.insert(file);
    return {};
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_map(Thread *thread, caddr_t addr,
                                          size_t len, sint prot, sint flags) {
  if (auto blockpool_map = thread->tproc->ops->blockpool_map) {
    return blockpool_map(thread, addr, len, prot, flags);
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_blockpool_unmap(Thread *thread, caddr_t addr,
                                            size_t len, sint flags) {
  if (auto blockpool_unmap = thread->tproc->ops->blockpool_unmap) {
    return blockpool_unmap(thread, addr, len);
  }
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
