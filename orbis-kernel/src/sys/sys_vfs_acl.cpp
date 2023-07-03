#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys___acl_get_file(Thread *thread, ptr<char> path, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_get_link(Thread *thread, ptr<const char> path, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_set_file(Thread *thread, ptr<char> path, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_set_link(Thread *thread, ptr<const char> path, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_get_fd(Thread *thread, sint filedes, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_set_fd(Thread *thread, sint filedes, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_delete_link(Thread *thread, ptr<const char> path, acl_type_t type) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_delete_file(Thread *thread, ptr<char> path, acl_type_t type) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_delete_fd(Thread *thread, sint filedes, acl_type_t type) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_aclcheck_file(Thread *thread, ptr<char> path, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_aclcheck_link(Thread *thread, ptr<const char> path, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys___acl_aclcheck_fd(Thread *thread, sint filedes, acl_type_t type, ptr<struct acl> aclp) { return ErrorCode::NOSYS; }
