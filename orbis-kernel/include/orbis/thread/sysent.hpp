#pragma once

#include "orbis-config.hpp"

namespace orbis {
struct Thread;
using sy_call_t = SysResult(Thread *, uint64_t *);

struct sysent {
  sint narg;
  sy_call_t *call;
};

struct sysentvec {
  sint size;
  const sysent *table;
};
} // namespace orbis
