#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_jail(Thread *thread, ptr<struct jail> jail) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_jail_set(Thread *thread, ptr<struct iovec> iovp, uint iovcnt, sint flags) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_jail_get(Thread *thread, ptr<struct iovec> iovp, uint iovcnt, sint flags) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_jail_remove(Thread *thread, sint jid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_jail_attach(Thread *thread, sint jid) { return ErrorCode::NOSYS; }
