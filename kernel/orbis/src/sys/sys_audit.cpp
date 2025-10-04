#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_audit(Thread *thread, ptr<const void> record,
                                  uint length) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_auditon(Thread *thread, sint cmd, ptr<void> data,
                                    uint length) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getauid(Thread *thread, ptr<uid_t> auid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setauid(Thread *thread, ptr<uid_t> auid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getaudit(Thread *thread,
                                     ptr<struct auditinfo> auditinfo) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setaudit(Thread *thread,
                                     ptr<struct auditinfo> auditinfo) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getaudit_addr(
    Thread *thread, ptr<struct auditinfo_addr> auditinfo_addr, uint length) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setaudit_addr(
    Thread *thread, ptr<struct auditinfo_addr> auditinfo_addr, uint length) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_auditctl(Thread *thread, ptr<char> path) {
  return ErrorCode::NOSYS;
}
