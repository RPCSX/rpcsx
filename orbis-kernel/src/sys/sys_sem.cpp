#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys___semctl(Thread *thread, sint semid, sint semnum, sint cmd, ptr<union semun> arg) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_semget(Thread *thread, key_t key, sint nsems, sint semflg) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_semop(Thread *thread, sint semid, ptr<struct sembuf> sops, size_t nspos) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_semsys(Thread *thread, sint which, sint a2, sint a3, sint a4, sint a5) { return ErrorCode::NOSYS; }
