#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_mount(Thread *thread, ptr<char> type, ptr<char> path, sint flags, caddr_t data) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_unmount(Thread *thread, ptr<char> path, sint flags) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_nmount(Thread *thread, ptr<struct iovec> iovp, uint iovcnt, sint flags) { return ErrorCode::NOSYS; }
