#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_thr_create(Thread *thread,
                                       ptr<struct ucontext> ctxt,
                                       ptr<slong> arg, sint flags) {
  if (auto thr_create = thread->tproc->ops->thr_create) {
    return thr_create(thread, ctxt, arg, flags);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_thr_new(Thread *thread, ptr<struct thr_param> param,
                                    sint param_size) {
  if (auto thr_new = thread->tproc->ops->thr_new) {
    return thr_new(thread, param, param_size);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_thr_self(Thread *thread, ptr<slong> id) {
  uwrite(id, (slong)thread->tid);
  return {};
}

orbis::SysResult orbis::sys_thr_exit(Thread *thread, ptr<slong> state) {
  if (auto thr_exit = thread->tproc->ops->thr_exit) {
    return thr_exit(thread, state);
  }
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_thr_kill(Thread *thread, slong id, sint sig) {
  if (auto thr_kill = thread->tproc->ops->thr_kill) {
    return thr_kill(thread, id, sig);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_thr_kill2(Thread *thread, pid_t pid, slong id,
                                      sint sig) {
  if (auto thr_kill2 = thread->tproc->ops->thr_kill2) {
    return thr_kill2(thread, pid, id, sig);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_thr_suspend(Thread *thread,
                                        ptr<const struct timespec> timeout) {
  if (auto thr_suspend = thread->tproc->ops->thr_suspend) {
    return thr_suspend(thread, timeout);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_thr_wake(Thread *thread, slong id) {
  if (auto thr_wake = thread->tproc->ops->thr_wake) {
    return thr_wake(thread, id);
  }

  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_thr_set_name(Thread *thread, slong id,
                                         ptr<const char> name) {
  if (auto thr_set_name = thread->tproc->ops->thr_set_name) {
    return thr_set_name(thread, id, name);
  }

  return ErrorCode::NOSYS;
}
