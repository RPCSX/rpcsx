#pragma once

#include "orbis-config.hpp"
#include <string>

namespace orbis {
struct Thread;
using sy_call_t = SysResult(Thread *, uint64_t *);

struct sysent {
  sint narg;
  sy_call_t *call;
  std::string (*format)(Thread *, uint64_t *);
};

struct sysentvec {
  sint size;
  const sysent *table;
};
} // namespace orbis
