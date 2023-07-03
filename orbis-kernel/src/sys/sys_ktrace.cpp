#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_ktrace(Thread *thread, ptr<const char> fname, sint ops, sint facs, sint pit) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_utrace(Thread *thread, ptr<const void> addr, size_t len) { return ErrorCode::NOSYS; }
