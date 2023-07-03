#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys___getcwd(Thread *thread, ptr<char> buf, uint buflen) { return ErrorCode::NOSYS; }
