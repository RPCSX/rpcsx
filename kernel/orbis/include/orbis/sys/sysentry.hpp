#pragma once

#include "orbis-config.hpp"
#include "orbis/thread/sysent.hpp"

namespace orbis {
extern sysentvec freebsd9_sysvec;
extern sysentvec freebsd11_sysvec;
extern sysentvec ps4_sysvec;
extern sysentvec ps5_sysvec;

struct Thread;
void syscall_entry(Thread *thread);
const char *getSysentName(SysResult (*sysent)(Thread *, uint64_t *));
} // namespace orbis
