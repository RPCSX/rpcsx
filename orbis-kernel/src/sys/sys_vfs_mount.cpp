#include "sys/sysproto.hpp"
#include "uio.hpp"
#include "utils/Logs.hpp"

orbis::SysResult orbis::sys_mount(Thread *thread, ptr<char> type,
                                  ptr<char> path, sint flags, caddr_t data) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_unmount(Thread *thread, ptr<char> path,
                                    sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_nmount(Thread *thread, ptr<IoVec> iovp, uint iovcnt,
                                   sint flags) {
  ORBIS_LOG_ERROR(__FUNCTION__, iovp, iovcnt, flags);

  for (auto it = iovp; it < iovp + iovcnt; it += 2) {
    IoVec a{}, b{};
    uread(a, it);
    uread(b, it + 1);

    std::string aSv((char *)a.base, a.len);
    std::string bSv((char *)b.base, b.len);

    std::fprintf(stderr, "%s: '%s':'%s'\n", __FUNCTION__, aSv.c_str(), bSv.c_str());
  }
  return {};
}
