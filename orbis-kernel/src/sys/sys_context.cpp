#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_getcontext(Thread *thread,
                                       ptr<UContext> ucp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setcontext(Thread *thread,
                                       ptr<UContext> ucp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_swapcontext(Thread *thread,
                                        ptr<UContext> oucp,
                                        ptr<UContext> ucp) {
  return ErrorCode::NOSYS;
}
