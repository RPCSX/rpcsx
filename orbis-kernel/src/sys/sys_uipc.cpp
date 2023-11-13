#include "pipe.hpp"
#include "sys/sysproto.hpp"
#include "uio.hpp"
#include "utils/Logs.hpp"
#include <chrono>
#include <sys/socket.h>
#include <thread>

orbis::SysResult orbis::sys_socket(Thread *thread, sint domain, sint type,
                                   sint protocol) {
  ORBIS_LOG_TODO(__FUNCTION__, domain, type, protocol);
  if (auto socket = thread->tproc->ops->socket) {
    Ref<File> file;
    auto result = socket(thread, nullptr, domain, type, protocol, &file);

    if (result.isError()) {
      return result;
    }

    auto fd = thread->tproc->fileDescriptors.insert(file);
    ORBIS_LOG_WARNING("Socket opened", fd);
    thread->retval[0] = fd;
    return {};
  }
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_bind(Thread *thread, sint s, caddr_t name,
                                 sint namelen) {
  ORBIS_LOG_ERROR(__FUNCTION__, s, name, namelen);
  return {};
}
orbis::SysResult orbis::sys_listen(Thread *thread, sint s, sint backlog) {
  ORBIS_LOG_ERROR(__FUNCTION__, s, backlog);
  return {};
}
orbis::SysResult orbis::sys_accept(Thread *thread, sint s,
                                   ptr<struct sockaddr> from,
                                   ptr<uint32_t> fromlenaddr) {
  ORBIS_LOG_ERROR(__FUNCTION__, s, from, fromlenaddr);
  std::this_thread::sleep_for(std::chrono::days(1));
  return SysResult::notAnError(ErrorCode::WOULDBLOCK);
}
orbis::SysResult orbis::sys_connect(Thread *thread, sint s, caddr_t name,
                                    sint namelen) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_socketpair(Thread *thread, sint domain, sint type,
                                       sint protocol, ptr<sint> rsv) {
  ORBIS_LOG_ERROR(__FUNCTION__, domain, type, protocol, rsv);

  auto pipe = createPipe();
  auto a = thread->tproc->fileDescriptors.insert(pipe);
  auto b = thread->tproc->fileDescriptors.insert(pipe);
  if (auto errc = uwrite(rsv, a); errc != ErrorCode{}) {
    return errc;
  }
  return uwrite(rsv + 1, b);
}
orbis::SysResult orbis::sys_sendto(Thread *thread, sint s, caddr_t buf,
                                   size_t len, sint flags, caddr_t to,
                                   sint tolen) {
  return {};
}
orbis::SysResult orbis::sys_sendmsg(Thread *thread, sint s,
                                    ptr<struct msghdr> msg, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_recvfrom(Thread *thread, sint s, caddr_t buf,
                                     size_t len, sint flags,
                                     ptr<struct sockaddr> from,
                                     ptr<uint32_t> fromlenaddr) {
  auto pipe = thread->tproc->fileDescriptors.get(s).cast<Pipe>();
  if (pipe == nullptr) {
    return ErrorCode::INVAL;
  }

  std::lock_guard lock(pipe->mtx);
  IoVec io = {
      .base = buf,
      .len = len,
  };
  Uio uio{
      .iov = &io,
      .iovcnt = 1,
      .segflg = UioSeg::UserSpace,
      .rw = UioRw::Read,
      .td = thread,
  };
  pipe->ops->read(pipe.get(), &uio, thread);
  thread->retval[0] = uio.offset;
  return {};
}
orbis::SysResult orbis::sys_recvmsg(Thread *thread, sint s,
                                    ptr<struct msghdr> msg, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_shutdown(Thread *thread, sint s, sint how) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_setsockopt(Thread *thread, sint s, sint level,
                                       sint name, caddr_t val, sint valsize) {
  ORBIS_LOG_TODO(__FUNCTION__, s, level, name, val, valsize);
  return {};
}
orbis::SysResult orbis::sys_getsockopt(Thread *thread, sint s, sint level,
                                       sint name, caddr_t val,
                                       ptr<sint> avalsize) {
  ORBIS_LOG_TODO(__FUNCTION__, s, level, name, val, avalsize);
  return {};
}
orbis::SysResult orbis::sys_getsockname(Thread *thread, sint fdes,
                                        ptr<struct sockaddr> asa,
                                        ptr<uint32_t> alen) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_getpeername(Thread *thread, sint fdes,
                                        ptr<struct sockaddr> asa,
                                        ptr<uint32_t> alen) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sendfile(Thread *thread, sint fd, sint s,
                                     off_t offset, size_t nbytes,
                                     ptr<struct sf_hdtr> hdtr,
                                     ptr<off_t> sbytes, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sctp_peeloff(Thread *thread, sint sd,
                                         uint32_t name) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_sctp_generic_sendmsg(Thread *thread, sint sd, caddr_t msg, sint mlen,
                                caddr_t to, __socklen_t tolen,
                                ptr<struct sctp_sndrcvinfo> sinfo, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sctp_generic_sendmsg_iov(
    Thread *thread, sint sd, ptr<IoVec> iov, sint iovlen, caddr_t to,
    __socklen_t tolen, ptr<struct sctp_sndrcvinfo> sinfo, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_sctp_generic_recvmsg(Thread *thread, sint sd, ptr<IoVec> iov,
                                sint iovlen, caddr_t from, __socklen_t fromlen,
                                ptr<struct sctp_sndrcvinfo> sinfo, sint flags) {
  return ErrorCode::NOSYS;
}
