#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_aio_return(Thread *thread, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_aio_suspend(Thread *thread, ptr<struct aiocb> aiocbp, sint nent, ptr<const struct timespec> timeout) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_aio_cancel(Thread *thread, sint fd, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_aio_error(Thread *thread, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_oaio_read(Thread *thread, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_aio_read(Thread *thread, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_oaio_write(Thread *thread, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_aio_write(Thread *thread, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_olio_listio(Thread *thread, sint mode, ptr<cptr<struct aiocb>> acb_list, sint nent, ptr<struct osigevent> sig) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_lio_listio(Thread *thread, sint mode, ptr<cptr<struct aiocb>> aiocbp, sint nent, ptr<struct sigevent> sig) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_aio_waitcomplete(Thread *thread, ptr<ptr<struct aiocb>> aiocbp, ptr<struct timespec> timeout) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_aio_fsync(Thread *thread, sint op, ptr<struct aiocb> aiocbp) { return ErrorCode::NOSYS; }

