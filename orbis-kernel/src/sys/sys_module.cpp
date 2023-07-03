#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_modnext(Thread *thread, sint modid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_modfnext(Thread *thread, sint modid) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_modstat(Thread *thread, sint modid, ptr<struct module_stat> stat) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_modfind(Thread *thread, ptr<const char> name) { return ErrorCode::NOSYS; }
