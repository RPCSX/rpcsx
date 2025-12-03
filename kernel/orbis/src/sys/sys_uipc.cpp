#include "file.hpp"
#include "sys/sysproto.hpp"
#include "thread/Process.hpp"
#include "thread/ProcessOps.hpp"
#include "thread/Thread.hpp"
#include "uio.hpp"
#include "utils/Logs.hpp"

orbis::SysResult orbis::sys_socket(Thread *thread, sint domain, sint type,
                                   sint protocol) {
  ORBIS_LOG_TODO(__FUNCTION__, domain, type, protocol);
  if (auto socket = thread->tproc->ops->socket) {
    rx::Ref<File> file;
    auto result = socket(thread, nullptr, domain, type, protocol, &file);

    if (result.isError()) {
      return result;
    }

    auto fd = thread->tproc->fileDescriptors.insert(file);
    ORBIS_LOG_WARNING("Socket opened", (int)fd);
    thread->retval[0] = std::to_underlying(fd);
    return {};
  }
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sys_bind(Thread *thread, FileDescriptor s, caddr_t name,
                                 sint namelen) {
  // ORBIS_LOG_ERROR(__FUNCTION__, (int)s, name, namelen);

  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto bind = file->ops->bind) {
    return bind(file.get(), ptr<SocketAddress>(name), namelen, thread);
  }

  return ErrorCode::NOTSUP;
}

orbis::SysResult orbis::sys_listen(Thread *thread, FileDescriptor s,
                                   sint backlog) {
  ORBIS_LOG_ERROR(__FUNCTION__, (int)s, backlog);
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto listen = file->ops->listen) {
    return listen(file.get(), backlog, thread);
  }

  return ErrorCode::NOTSUP;
}

orbis::SysResult orbis::sys_accept(Thread *thread, FileDescriptor s,
                                   ptr<SocketAddress> from,
                                   ptr<uint32_t> fromlenaddr) {
  ORBIS_LOG_ERROR(__FUNCTION__, (int)s, from, fromlenaddr);
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto accept = file->ops->accept) {
    return accept(file.get(), from, fromlenaddr, thread);
  }

  return ErrorCode::NOTSUP;
}

orbis::SysResult orbis::sys_connect(Thread *thread, FileDescriptor s,
                                    caddr_t name, sint namelen) {
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto connect = file->ops->connect) {
    return connect(file.get(), ptr<SocketAddress>(name), namelen, thread);
  }

  return ErrorCode::NOTSUP;
}
orbis::SysResult orbis::sys_socketpair(Thread *thread, sint domain, sint type,
                                       sint protocol, ptr<sint> rsv) {
  ORBIS_LOG_TODO(__FUNCTION__, domain, type, protocol, rsv);
  if (auto socketpair = thread->tproc->ops->socketpair) {
    rx::Ref<File> a;
    rx::Ref<File> b;
    auto result = socketpair(thread, domain, type, protocol, &a, &b);

    if (result.isError()) {
      return result;
    }

    auto aFd = thread->tproc->fileDescriptors.insert(a);
    auto bFd = thread->tproc->fileDescriptors.insert(b);

    ORBIS_RET_ON_ERROR(uwrite(rsv, std::to_underlying(aFd)));

    return uwrite(rsv + 1, std::to_underlying(bFd));
  }

  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sendto(Thread *thread, FileDescriptor s,
                                   caddr_t buf, size_t len, sint flags,
                                   caddr_t to, sint tolen) {
  return {};
}
orbis::SysResult orbis::sys_sendmsg(Thread *thread, FileDescriptor s,
                                    ptr<struct msghdr> msg, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_recvfrom(Thread *thread, FileDescriptor s,
                                     caddr_t buf, size_t len, sint flags,
                                     ptr<SocketAddress> from,
                                     ptr<uint32_t> fromlenaddr) {
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto recvfrom = file->ops->recvfrom) {
    return SysResult::notAnError(
        recvfrom(file.get(), buf, len, flags, from, fromlenaddr, thread));
  }

  return ErrorCode::NOTSUP;
}
orbis::SysResult orbis::sys_recvmsg(Thread *thread, FileDescriptor s,
                                    ptr<struct msghdr> msg, sint flags) {
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto recvmsg = file->ops->recvmsg) {
    return recvmsg(file.get(), msg, flags, thread);
  }

  return ErrorCode::NOTSUP;
}
orbis::SysResult orbis::sys_shutdown(Thread *thread, FileDescriptor s,
                                     sint how) {
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto shutdown = file->ops->shutdown) {
    return shutdown(file.get(), how, thread);
  }

  return ErrorCode::NOTSUP;
}
orbis::SysResult orbis::sys_setsockopt(Thread *thread, FileDescriptor s,
                                       sint level, sint name, caddr_t val,
                                       sint valsize) {
  ORBIS_LOG_TODO(__FUNCTION__, (int)s, level, name, val, valsize);
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto setsockopt = file->ops->setsockopt) {
    return setsockopt(file.get(), level, name, val, valsize, thread);
  }

  return ErrorCode::NOTSUP;
}
orbis::SysResult orbis::sys_getsockopt(Thread *thread, FileDescriptor s,
                                       sint level, sint name, caddr_t val,
                                       ptr<sint> avalsize) {
  ORBIS_LOG_TODO(__FUNCTION__, (int)s, level, name, val, avalsize);
  auto file = thread->tproc->fileDescriptors.get(s);
  if (file == nullptr) {
    return ErrorCode::BADF;
  }

  if (auto getsockopt = file->ops->getsockopt) {
    return getsockopt(file.get(), level, name, val, avalsize, thread);
  }

  return ErrorCode::NOTSUP;
}
orbis::SysResult orbis::sys_getsockname(Thread *thread, FileDescriptor fdes,
                                        ptr<SocketAddress> asa,
                                        ptr<uint32_t> alen) {
  // return uwrite<uint32_t>(alen, sizeof(SockAddr));
  ORBIS_LOG_TODO(__FUNCTION__);
  return {};
}
orbis::SysResult orbis::sys_getpeername(Thread *thread, FileDescriptor fdes,
                                        ptr<SocketAddress> asa,
                                        ptr<uint32_t> alen) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sendfile(Thread *thread, FileDescriptor fd,
                                     FileDescriptor s, off_t offset,
                                     size_t nbytes, ptr<struct sf_hdtr> hdtr,
                                     ptr<off_t> sbytes, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sctp_peeloff(Thread *thread, sint sd,
                                         uint32_t name) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_sctp_generic_sendmsg(Thread *thread, sint sd, caddr_t msg, sint mlen,
                                caddr_t to, SockLen tolen,
                                ptr<struct sctp_sndrcvinfo> sinfo, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sys_sctp_generic_sendmsg_iov(
    Thread *thread, sint sd, ptr<IoVec> iov, sint iovlen, caddr_t to,
    SockLen tolen, ptr<struct sctp_sndrcvinfo> sinfo, sint flags) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sys_sctp_generic_recvmsg(Thread *thread, sint sd, ptr<IoVec> iov,
                                sint iovlen, caddr_t from, SockLen fromlen,
                                ptr<struct sctp_sndrcvinfo> sinfo, sint flags) {
  return ErrorCode::NOSYS;
}
