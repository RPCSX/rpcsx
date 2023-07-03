#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_msgctl(Thread *thread, sint msqid, sint cmd, ptr<struct msqid_ds> buf) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_msgget(Thread *thread, key_t key, sint msgflg) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_msgsnd(Thread *thread, sint msqid, ptr<const void> msgp, size_t msgsz, sint msgflg) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_msgrcv(Thread *thread, sint msqid, ptr<void> msgp, size_t msgsz, slong msgtyp, sint msgflg) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_msgsys(Thread *thread, sint which, sint a2, sint a3, sint a4, sint a5, sint a6) { return ErrorCode::NOSYS; }
