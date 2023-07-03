#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys__umtx_lock(Thread *thread, ptr<struct umtx> umtx) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys__umtx_unlock(Thread *thread, ptr<struct umtx> umtx) { return ErrorCode::NOSYS; }
orbis::SysResult orbis::sys__umtx_op(Thread *thread, ptr<void> obj, sint op, ulong val, ptr<void> uaddr1, ptr<void> uaddr2) {
  std::printf("TODO: sys__umtx_op\n");
  return {};
}
