#include "sys/sysproto.hpp"
#include "time.hpp"
#include "utils/Logs.hpp"
#include <ctime>

enum class ClockId {
  Realtime = 0,
  Virtual = 1,
  Prof = 2,
  Monotonic = 4,
  Uptime = 5,            // FreeBSD-specific
  UptimePrecise = 7,     // FreeBSD-specific
  UptimeFast = 8,        // FreeBSD-specific
  RealtimePrecise = 9,   // FreeBSD-specific
  RealtimeFast = 10,     // FreeBSD-specific
  MonotonicPrecise = 11, // FreeBSD-specific
  MonotonicFast = 12,    // FreeBSD-specific
  Second = 13,           // FreeBSD-specific
  ThreadCputimeId = 14,

  // orbis extension
  Proctime = 15,
  Network = 16,
  DebugNetwork = 17,
  AdNetwork = 18,
  RawNetwork = 19,
};

orbis::SysResult orbis::sys_clock_gettime(Thread *, clockid_t clock_id,
                                          ptr<timespec> tp) {
  timespec result{};

  auto getHostClock = [](clockid_t clock_id) {
    ::timespec hostClock;
    ::clock_gettime(clock_id, &hostClock);

    timespec result;
    result.sec = hostClock.tv_sec;
    result.nsec = hostClock.tv_nsec;
    return result;
  };

  switch (static_cast<ClockId>(clock_id)) {
  case ClockId::Realtime:
  case ClockId::RealtimePrecise:
    result = getHostClock(CLOCK_REALTIME);
    break;

  case ClockId::RealtimeFast: {
    result = getHostClock(CLOCK_REALTIME); // FIXME
    break;
  }
  case ClockId::Virtual:
    ORBIS_LOG_ERROR("Unimplemented ClockId::Virtual\n");
    break;
  case ClockId::Prof:
    ORBIS_LOG_ERROR("Unimplemented ClockId::Prof\n");
    break;

  case ClockId::Monotonic:
  case ClockId::MonotonicPrecise:
  case ClockId::Uptime:
  case ClockId::UptimePrecise: {
    result = getHostClock(CLOCK_MONOTONIC);
    break;
  }

  case ClockId::UptimeFast:
  case ClockId::MonotonicFast:
    result = getHostClock(CLOCK_MONOTONIC); // FIXME
    break;

  case ClockId::Second: {
    result = getHostClock(CLOCK_MONOTONIC); // FIXME
    result.nsec = 0;
    break;
  }
  case ClockId::ThreadCputimeId:
    result = getHostClock(CLOCK_THREAD_CPUTIME_ID);
    break;
  case ClockId::Proctime:
    result = getHostClock(CLOCK_PROCESS_CPUTIME_ID);
    break;
  case ClockId::Network:
    ORBIS_LOG_ERROR("Unimplemented ClockId::Network\n");
    break;
  case ClockId::DebugNetwork:
    ORBIS_LOG_ERROR("Unimplemented ClockId::DebugNetwork\n");
    break;
  case ClockId::AdNetwork:
    ORBIS_LOG_ERROR("Unimplemented ClockId::AdNetwork\n");
    break;
  case ClockId::RawNetwork:
    ORBIS_LOG_ERROR("Unimplemented ClockId::RawNetwork\n");
    break;

  default:
    return ErrorCode::INVAL;
  }

  return uwrite(tp, result);
}
orbis::SysResult orbis::sys_clock_settime(Thread *thread, clockid_t clock_id,
                                          ptr<const timespec> tp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_clock_getres(Thread *thread, clockid_t clock_id,
                                         ptr<timespec> tp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_nanosleep(Thread *thread,
                                      cptr<orbis::timespec> rqtp,
                                      ptr<orbis::timespec> rmtp) {
  ORBIS_LOG_TRACE(__FUNCTION__, rqtp, rmtp);
  struct ::timespec rq;
  struct ::timespec rm;
  orbis::timespec value;
  if (auto e = uread(value, rqtp); e != ErrorCode{})
    return e;
  rq.tv_sec = value.sec;
  rq.tv_nsec = value.nsec;
  if (::nanosleep(&rq, &rm) == EINTR) {
    if (rmtp) {
      value.sec = rm.tv_sec;
      value.nsec = rm.tv_nsec;
      if (auto e = uwrite(rmtp, value); e != ErrorCode{})
        return e;
    }
    return ErrorCode::INTR;
  }
  return {};
}
orbis::SysResult orbis::sys_gettimeofday(Thread *thread, ptr<orbis::timeval> tp,
                                         ptr<orbis::timezone> tzp) {
  ORBIS_LOG_TRACE(__FUNCTION__, tp, tzp);
  struct ::timeval tv;
  if (::gettimeofday(&tv, nullptr) != 0)
    std::abort();
  if (tp) {
    orbis::timeval value;
    value.tv_sec = tv.tv_sec;
    value.tv_usec = tv.tv_usec;
    if (auto e = uwrite(tp, value); e != ErrorCode{})
      return e;
  }
  if (tzp) {
    struct ::tm tp;
    if (localtime_r(&tv.tv_sec, &tp) != &tp)
      std::abort();
    orbis::timezone value;
    value.tz_dsttime = 0; // TODO
    value.tz_mineast = tp.tm_gmtoff / 60;
    if (auto e = uwrite(tzp, value); e != ErrorCode{})
      return e;
  }
  return {};
}
orbis::SysResult orbis::sys_settimeofday(Thread *thread, ptr<struct timeval> tp,
                                         ptr<orbis::timezone> tzp) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getitimer(Thread *thread, uint which,
                                      ptr<struct itimerval> itv) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setitimer(Thread *thread, uint which,
                                      ptr<struct itimerval> itv,
                                      ptr<struct itimerval> oitv) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ktimer_create(Thread *thread, clockid_t clock_id,
                                          ptr<struct sigevent> evp,
                                          ptr<sint> timerid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ktimer_delete(Thread *thread, sint timerid) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ktimer_settime(Thread *thread, sint timerid,
                                           sint flags,
                                           ptr<const struct itimerspec> value,
                                           ptr<struct itimerspec> ovalue) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ktimer_gettime(Thread *thread, sint timerid,
                                           ptr<struct itimerspec> value) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_ktimer_getoverrun(Thread *thread, sint timerid) {
  return ErrorCode::NOSYS;
}
