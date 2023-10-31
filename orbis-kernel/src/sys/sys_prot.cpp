#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"

orbis::SysResult orbis::sys_getpid(Thread *thread) {
  thread->retval[0] = thread->tid;
  return {};
}
orbis::SysResult orbis::sys_getppid(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getpgrp(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getpgid(Thread *thread, pid_t pid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getsid(Thread *thread, pid_t pid) {
  return {};
}
orbis::SysResult orbis::sys_getuid(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_geteuid(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getgid(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getegid(Thread *thread) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_getgroups(Thread *thread, uint gidsetsize,
                                      ptr<gid_t> gidset) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setsid(Thread *thread) {
  ORBIS_LOG_WARNING(__FUNCTION__);
  return {};
}
orbis::SysResult orbis::sys_setpgid(Thread *thread, sint pid, sint pgid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setuid(Thread *thread, uid_t uid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_seteuid(Thread *thread, uid_t euid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setgid(Thread *thread, gid_t gid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setegid(Thread *thread, gid_t egid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setgroups(Thread *thread, uint gidsetsize,
                                      ptr<gid_t> gidset) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setreuid(Thread *thread, sint ruid, sint euid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setregid(Thread *thread, sint rgid, sint egid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setresuid(Thread *thread, uid_t ruid, uid_t euid,
                                      uid_t suid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setresgid(Thread *thread, gid_t rgid, gid_t egid,
                                      gid_t sgid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getresuid(Thread *thread, ptr<uid_t> ruid,
                                      ptr<uid_t> euid, ptr<uid_t> suid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getresgid(Thread *thread, ptr<gid_t> rgid,
                                      ptr<gid_t> egid, ptr<gid_t> sgid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_issetugid(Thread *thread) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys___setugid(Thread *thread, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getlogin(Thread *thread, ptr<char> namebuf,
                                     uint namelen) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setlogin(Thread *thread, ptr<char> namebuf) {
  ORBIS_LOG_WARNING(__FUNCTION__, namebuf);
  return {};
}
