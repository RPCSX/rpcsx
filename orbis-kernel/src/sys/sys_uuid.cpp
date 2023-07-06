#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_uuidgen(Thread *thread, ptr<struct uuid> store,
                                    sint count) {
  return ErrorCode::NOSYS;
}
