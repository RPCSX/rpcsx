#include "ipmi.hpp"
#include "KernelContext.hpp"
#include "thread/Process.hpp"

orbis::ErrorCode orbis::ipmiCreateClient(Thread *thread, void *clientImpl,
                                         const char *name, void *config,
                                         Ref<IpmiClient> &result) {
  auto client = knew<IpmiClient>(name);
  if (client == nullptr) {
    return ErrorCode::NOMEM;
  }

  client->clientImpl = clientImpl;
  client->name = name;
  client->pid = thread->tproc->pid;
  result = client;
  return {};
}

orbis::ErrorCode orbis::ipmiCreateServer(Thread *thread, void *serverImpl,
                                         const char *name,
                                         const IpmiCreateServerConfig &config,
                                         Ref<IpmiServer> &result) {
  auto [server, errorCode] = g_context.createIpmiServer(name);
  ORBIS_RET_ON_ERROR(errorCode);

  server->serverImpl = serverImpl;
  server->userData = config.userData;
  server->eventHandler = config.eventHandler;
  server->pid = thread->tproc->pid;
  result = server;
  return {};
}

orbis::ErrorCode orbis::ipmiCreateSession(Thread *thread, void *sessionImpl,
                                          ptr<void> userData,
                                          Ref<IpmiSession> &result) {
  std::unique_lock ipmiMapLock(thread->tproc->ipmiMap.mutex);

  for (auto [kid, obj] : thread->tproc->ipmiMap) {
    auto server = dynamic_cast<IpmiServer *>(obj);
    if (server == nullptr) {
      continue;
    }

    std::lock_guard serverLock(server->mutex);
    for (auto &conReq : server->connectionRequests) {
      if (conReq.serverTid != thread->tid ||
          conReq.client->session != nullptr) {
        continue;
      }

      std::lock_guard clientLock(conReq.client->mutex);
      if (conReq.client->session != nullptr) {
        continue;
      }

      auto session = knew<IpmiSession>();
      if (session == nullptr) {
        return ErrorCode::NOMEM;
      }

      result = session;
      session->sessionImpl = sessionImpl;
      session->userData = userData;
      session->client = conReq.client;
      session->server = server;
      conReq.client->session = session;

      return {};
    }
  }

  ORBIS_LOG_ERROR(__FUNCTION__, ": connection request not found");
  return ErrorCode::INVAL;
}

orbis::SysResult orbis::sysIpmiCreateClient(Thread *thread, ptr<uint> result,
                                            ptr<void> params,
                                            uint64_t paramsSz) {
  struct IpmiCreateClientParams {
    orbis::ptr<void> clientImpl;
    orbis::ptr<const char> name;
    orbis::ptr<void> config; // FIXME: describe
  };

  static_assert(sizeof(IpmiCreateClientParams) == 0x18);

  if (paramsSz != sizeof(IpmiCreateClientParams)) {
    return ErrorCode::INVAL;
  }

  IpmiCreateClientParams _params;
  char _name[25];
  Ref<IpmiClient> client;

  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiCreateClientParams>(params)));
  ORBIS_RET_ON_ERROR(ureadString(_name, sizeof(_name), _params.name));
  ORBIS_RET_ON_ERROR(
      ipmiCreateClient(thread, _params.clientImpl, _name, nullptr, client));

  auto kid = thread->tproc->ipmiMap.insert(std::move(client));

  if (kid == -1) {
    return ErrorCode::MFILE;
  }

  return uwrite<uint>(result, kid);
}

orbis::SysResult orbis::sysIpmiCreateServer(Thread *thread, ptr<uint> result,
                                            ptr<void> params,
                                            uint64_t paramsSz) {
  struct IpmiCreateServerParams {
    orbis::ptr<void> serverImpl;
    orbis::ptr<const char> name;
    orbis::ptr<IpmiCreateServerConfig> config;
  };

  static_assert(sizeof(IpmiCreateServerParams) == 0x18);

  if (paramsSz != sizeof(IpmiCreateServerParams)) {
    return ErrorCode::INVAL;
  }

  IpmiCreateServerParams _params;
  IpmiCreateServerConfig _config;
  char _name[25];
  Ref<IpmiServer> server;

  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiCreateServerParams>(params)));
  ORBIS_RET_ON_ERROR(uread(_config, _params.config));
  ORBIS_RET_ON_ERROR(ureadString(_name, sizeof(_name), _params.name));
  ORBIS_RET_ON_ERROR(
      ipmiCreateServer(thread, _params.serverImpl, _name, _config, server));
  auto kid = thread->tproc->ipmiMap.insert(std::move(server));

  if (kid == -1) {
    return ErrorCode::MFILE;
  }

  return uwrite<uint>(result, kid);
}

orbis::SysResult orbis::sysIpmiCreateSession(Thread *thread, ptr<uint> result,
                                             ptr<void> params,
                                             uint64_t paramsSz) {
  struct IpmiSessionUserData {
    uint64_t size;
    ptr<void> data;
  };

  static_assert(sizeof(IpmiSessionUserData) == 0x10);

  struct IpmiCreateSessionParams {
    ptr<void> sessionImpl;
    ptr<IpmiSessionUserData> userData;
  };

  static_assert(sizeof(IpmiCreateSessionParams) == 0x10);

  if (paramsSz != sizeof(IpmiCreateSessionParams)) {
    return ErrorCode::INVAL;
  }

  IpmiCreateSessionParams _params;
  IpmiSessionUserData _userData;
  Ref<IpmiSession> session;

  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiCreateSessionParams>(params)));
  ORBIS_RET_ON_ERROR(uread(_userData, _params.userData));

  if (_userData.size != sizeof(IpmiSessionUserData)) {
    return ErrorCode::INVAL;
  }

  ORBIS_RET_ON_ERROR(
      ipmiCreateSession(thread, _params.sessionImpl, _userData.data, session));

  auto kid = thread->tproc->ipmiMap.insert(std::move(session));

  if (kid == -1) {
    return ErrorCode::MFILE;
  }

  return uwrite<uint>(result, kid);
}

orbis::SysResult orbis::sysIpmiDestroyClient(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sysIpmiDestroyServer(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sysIpmiDestroySession(Thread *thread, ptr<uint> result,
                                              uint kid, ptr<void> params,
                                              uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}

orbis::SysResult orbis::sysIpmiServerReceivePacket(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sysIpmiSessionConnectResult(Thread *thread,
                                                    ptr<uint> result, uint kid,
                                                    ptr<void> params,
                                                    uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sysIpmiSessionRespondSync(Thread *thread,
                                                  ptr<uint> result, uint kid,
                                                  ptr<void> params,
                                                  uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sysIpmiSessionGetClientPid(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult
orbis::sysIpmiClientInvokeSyncMethod(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sysIpmiClientConnect(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
orbis::SysResult orbis::sysIpmiServerGetName(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  return ErrorCode::NOSYS;
}
