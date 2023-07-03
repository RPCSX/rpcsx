#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_kldload(Thread *thread, ptr<const char> file) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_kldunload(Thread *thread, sint fileid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_kldunloadf(Thread *thread, slong fileid, sint flags) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_kldfind(Thread *thread, ptr<const char> name) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_kldnext(Thread *thread, sint fileid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_kldstat(Thread *thread, sint fileid, ptr<struct kld_file_stat> stat) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_kldfirstmod(Thread *thread, sint fileid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_kldsym(Thread *thread, sint fileid, sint cmd, ptr<void> data) { return ErrorCode::NOSYS; }
