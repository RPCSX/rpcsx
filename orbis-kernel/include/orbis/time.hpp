#pragma once

#include "orbis-config.hpp"

namespace orbis {
struct timespec {
  uint64_t sec;
  uint64_t nsec;
};
struct timeval {
  int64_t tv_sec;
  int64_t tv_usec;
};
struct timezone {
  sint tz_minuteswest;
  sint tz_dsttime;
};
} // namespace orbis
