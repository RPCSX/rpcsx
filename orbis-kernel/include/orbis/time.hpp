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
struct timesec {
  int64_t tz_time;
  sint tz_secwest;
  sint tz_dstsec;
};
} // namespace orbis
