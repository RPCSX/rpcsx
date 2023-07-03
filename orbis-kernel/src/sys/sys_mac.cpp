#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys___mac_get_pid(Thread *thread, pid_t pid, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_get_proc(Thread *thread, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_set_proc(Thread *thread, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_get_fd(Thread *thread, sint fd, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_get_file(Thread *thread, ptr<const char> path, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_set_fd(Thread *thread, sint fd, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_set_file(Thread *thread, ptr<const char> path, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_get_link(Thread *thread, ptr<const char> path_p, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___mac_set_link(Thread *thread, ptr<const char> path_p, ptr<struct mac> mac_p) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_mac_syscall(Thread *thread, ptr<const char> policy, sint call, ptr<void> arg) { return ErrorCode::NOSYS; }
