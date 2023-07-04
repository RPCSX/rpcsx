#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_socket(Thread *thread, sint domain, sint type, sint protocol) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_bind(Thread *thread, sint s, caddr_t name, sint namelen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_listen(Thread *thread, sint s, sint backlog) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_accept(Thread *thread, sint s, ptr<struct sockaddr> from, ptr<uint32_t> fromlenaddr) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_connect(Thread *thread, sint s, caddr_t name, sint namelen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_socketpair(Thread *thread, sint domain, sint type, sint protocol, ptr<sint> rsv) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_sendto(Thread *thread, sint s, caddr_t buf, size_t len, sint flags, caddr_t to, sint tolen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_sendmsg(Thread *thread, sint s, ptr<struct msghdr> msg, sint flags) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_recvfrom(Thread *thread, sint s, caddr_t buf, size_t len, sint flags, ptr<struct sockaddr> from, ptr<uint32_t> fromlenaddr) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_recvmsg(Thread *thread, sint s, ptr<struct msghdr> msg, sint flags) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_shutdown(Thread *thread, sint s, sint how) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_setsockopt(Thread *thread, sint s, sint level, sint name, caddr_t val, sint valsize) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getsockopt(Thread *thread, sint s, sint level, sint name, caddr_t val, ptr<sint> avalsize) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getsockname(Thread *thread, sint fdes, ptr<struct sockaddr> asa, ptr<uint32_t> alen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getpeername(Thread *thread, sint fdes, ptr<struct sockaddr> asa, ptr<uint32_t> alen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_sendfile(Thread *thread, sint fd, sint s, off_t offset, size_t nbytes, ptr<struct sf_hdtr> hdtr, ptr<off_t> sbytes, sint flags) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_sctp_peeloff(Thread *thread, sint sd, uint32_t name) { return ErrorCode::NOSYS; }
