#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_ksem_init(Thread *thread, ptr<semid_t> idp, uint value) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_open(Thread *thread, ptr<semid_t> idp, ptr<const char> name, sint oflag, mode_t mode, uint value) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_unlink(Thread *thread, ptr<const char> name) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_close(Thread *thread, semid_t id) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_post(Thread *thread, semid_t id) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_wait(Thread *thread, semid_t id) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_timedwait(Thread *thread, semid_t id, ptr<const struct timespec> abstime) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_trywait(Thread *thread, semid_t id) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_getvalue(Thread *thread, semid_t id, ptr<sint> value) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_ksem_destroy(Thread *thread, semid_t id) { return ErrorCode::NOSYS; }
