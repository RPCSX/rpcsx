#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_extattrctl(Thread *thread, ptr<char> path, char cmd, ptr<const char> filename, sint attrnamespace, ptr<const char> attrname) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_set_fd(Thread *thread, sint fd, sint attrnamespace, ptr<const char> attrname, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_set_file(Thread *thread, ptr<char> path, sint attrnamespace, ptr<const char> filename, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_set_link(Thread *thread, ptr<const char> path, sint attrnamespace, ptr<const char> attrname, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_get_fd(Thread *thread, sint fd, sint attrnamespace, ptr<const char> attrname, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_get_file(Thread *thread, ptr<char> path, sint attrnamespace, ptr<const char> filename, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_get_link(Thread *thread, ptr<const char> path, sint attrnamespace, ptr<const char> attrname, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_delete_fd(Thread *thread, sint fd, sint attrnamespace, ptr<const char> attrname) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_delete_file(Thread *thread, ptr<char> path, sint attrnamespace, ptr<const char> attrname) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_delete_link(Thread *thread, ptr<const char> path, sint attrnamespace, ptr<const char> attrname) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_list_fd(Thread *thread, sint fd, sint attrnamespace, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_list_file(Thread *thread, ptr<const char> path, sint attrnamespace, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_extattr_list_link(Thread *thread, ptr<const char> path, sint attrnamespace, ptr<void> data, size_t nbytes) { return ErrorCode::NOSYS; }
