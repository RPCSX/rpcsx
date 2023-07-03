#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_rctl_get_racct(Thread *thread, ptr<const void> inbufp, size_t inbuflen, ptr<void> outbuf, size_t outbuflen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rctl_get_rules(Thread *thread, ptr<const void> inbufp, size_t inbuflen, ptr<void> outbuf, size_t outbuflen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rctl_get_limits(Thread *thread, ptr<const void> inbufp, size_t inbuflen, ptr<void> outbuf, size_t outbuflen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rctl_add_rule(Thread *thread, ptr<const void> inbufp, size_t inbuflen, ptr<void> outbuf, size_t outbuflen) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys_rctl_remove_rule(Thread *thread, ptr<const void> inbufp, size_t inbuflen, ptr<void> outbuf, size_t outbuflen) { return ErrorCode::NOSYS; }
