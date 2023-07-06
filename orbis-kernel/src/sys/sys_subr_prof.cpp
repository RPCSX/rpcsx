#include "sys/sysproto.hpp"

orbis::SysResult orbis::sys_profil(Thread *thread, caddr_t samples, size_t size,
                                   size_t offset, uint scale) {
  return ErrorCode::NOSYS;
}
