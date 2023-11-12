#include "ipmi.hpp"
#include "KernelContext.hpp"
#include "thread/Process.hpp"
#include "utils/Logs.hpp"

namespace orbis {
struct IpmiBufferInfo {
  ptr<void> data;
  uint64_t size;
};

static_assert(sizeof(IpmiBufferInfo) == 0x10);

struct IpmiDataInfo {
  ptr<void> data;
  uint64_t size;
  uint64_t capacity; //?
};

static_assert(sizeof(IpmiDataInfo) == 0x18);
} // namespace orbis

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
      conReq.client->sessionCv.notify_all(conReq.client->mutex);

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
    ptr<void> clientImpl;
    ptr<const char> name;
    ptr<void> config; // FIXME: describe
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

  ORBIS_LOG_ERROR(__FUNCTION__, kid, _name);
  return uwrite<uint>(result, kid);
}

orbis::SysResult orbis::sysIpmiCreateServer(Thread *thread, ptr<uint> result,
                                            ptr<void> params,
                                            uint64_t paramsSz) {
  struct IpmiCreateServerParams {
    ptr<void> serverImpl;
    ptr<const char> name;
    ptr<IpmiCreateServerConfig> config;
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

  ORBIS_LOG_ERROR(__FUNCTION__, kid, _name);
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

  ORBIS_LOG_ERROR(__FUNCTION__, kid);
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
  struct IpmiServerReceivePacketParams {
    ptr<void> buffer;
    uint64_t bufferSize;
    ptr<IpmiServer::IpmiPacketInfo> receivePacketInfo;
    ptr<uint> unk;
  };

  IpmiServerReceivePacketParams _params;

  ORBIS_RET_ON_ERROR(
      uread(_params, ptr<IpmiServerReceivePacketParams>(params)));

  auto server = thread->tproc->ipmiMap.get(kid).cast<IpmiServer>();

  if (server == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiServer::Packet _packet;

  ORBIS_LOG_ERROR(__FUNCTION__, server->name, ": waiting for packet");
  {
    std::lock_guard lock(server->mutex);
    while (server->packets.empty()) {
      server->receiveCv.wait(server->mutex);
    }

    _packet = std::move(server->packets.front());
    server->packets.pop_front();
  }
  ORBIS_LOG_ERROR(__FUNCTION__, server->name, ": got packet");

  if (_packet.info.type == 0x1) {
    // on connection packet

    for (auto &conn : server->connectionRequests) {
      if ((ptr<void>)conn.clientTid == _packet.info.userData) {
        conn.serverTid = thread->tid;
        _packet.info.userData = nullptr;
        break;
      }
    }
  }

  ORBIS_RET_ON_ERROR(uwriteRaw((ptr<std::byte>)_params.buffer,
                               _packet.message.data(), _packet.message.size()));
  _params.bufferSize = _packet.message.size();
  _packet.info.eventHandler = server->eventHandler;
  ORBIS_RET_ON_ERROR(uwrite(_params.receivePacketInfo, _packet.info));
  ORBIS_RET_ON_ERROR(
      uwrite(ptr<IpmiServerReceivePacketParams>(params), _params));

  return uwrite<uint>(result, 0);
}
orbis::SysResult orbis::sysIpmiSendConnectResult(Thread *thread,
                                                 ptr<uint> result, uint kid,
                                                 ptr<void> params,
                                                 uint64_t paramsSz) {
  if (paramsSz != sizeof(sint)) {
    ORBIS_LOG_ERROR(__FUNCTION__, "wrong param size");
    return ErrorCode::INVAL;
  }

  sint status;
  ORBIS_RET_ON_ERROR(uread(status, ptr<sint>(params)));

  ORBIS_LOG_NOTICE(__FUNCTION__, kid, status);
  return uwrite(result, 0u);
}
orbis::SysResult orbis::sysIpmiSessionRespondSync(Thread *thread,
                                                  ptr<uint> result, uint kid,
                                                  ptr<void> params,
                                                  uint64_t paramsSz) {
  struct IpmiRespondParams {
    sint errorCode;
    uint32_t unk1;
    ptr<IpmiBufferInfo> buffers;
    uint32_t bufferCount;
    uint32_t padding;
  };

  static_assert(sizeof(IpmiRespondParams) == 0x18);
  if (paramsSz != sizeof(IpmiRespondParams)) {
    return ErrorCode::INVAL;
  }

  auto session = thread->tproc->ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiRespondParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiRespondParams>(params)));

  if (_params.bufferCount > 1) {
    ORBIS_LOG_ERROR(__FUNCTION__, "unexpected buffers count");
    return ErrorCode::INVAL;
  }

  ORBIS_LOG_ERROR(__FUNCTION__, session->client->name);

  kvector<std::byte> data;

  if (_params.errorCode == 0 && _params.bufferCount > 0 &&
      session->expectedOutput) {
    IpmiBufferInfo _buffer;
    ORBIS_RET_ON_ERROR(uread(_buffer, _params.buffers));

    data.resize(_buffer.size);
    ORBIS_RET_ON_ERROR(ureadRaw(data.data(), _buffer.data, _buffer.size));
  }

  std::lock_guard lock(session->mutex);

  session->messageResponses.push_front({
      .errorCode = _params.errorCode,
      .data = std::move(data),
  });

  session->responseCv.notify_one(session->mutex);
  return uwrite(result, 0u);
}

orbis::SysResult orbis::sysIpmiSessionGetClientPid(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  struct IpmiGetClientPidParams {
    ptr<uint32_t> pid;
  };

  if (paramsSz != sizeof(IpmiGetClientPidParams)) {
    return ErrorCode::INVAL;
  }

  auto session = thread->tproc->ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiGetClientPidParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiGetClientPidParams>(params)));
  ORBIS_RET_ON_ERROR(uwrite<uint32_t>(_params.pid, session->client->pid));
  return uwrite<uint>(result, 0);
}
orbis::SysResult
orbis::sysIpmiClientInvokeSyncMethod(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz) {
  struct IpmiSyncCallParams {
    uint32_t method;
    uint32_t numInData;
    uint32_t numOutData;
    uint32_t unk;
    ptr<IpmiBufferInfo> pInData;
    ptr<IpmiDataInfo> pOutData;
    ptr<sint> pResult;
    uint32_t flags;
  };

  static_assert(sizeof(IpmiSyncCallParams) == 0x30);

  struct MessageHeader {
    ptr<void> sessionImpl;
    uint pid;
    uint methodId;
    uint numInData;
    uint numOutData;
  };

  static_assert(sizeof(MessageHeader) == 0x18);

  if (paramsSz != sizeof(IpmiSyncCallParams)) {
    return ErrorCode::INVAL;
  }

  IpmiSyncCallParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, (ptr<IpmiSyncCallParams>)params));

  if (_params.flags > 1) {
    return ErrorCode::INVAL;
  }

  auto client = thread->tproc->ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  std::lock_guard clientLock(client->mutex);
  auto session = client->session;

  if (session == nullptr) {
    ORBIS_LOG_TODO(__FUNCTION__, "waiting for connection", client->name,
                   _params.method);

    while (session == nullptr) {
      client->sessionCv.wait(client->mutex);
      session = client->session;
    }
  }

  ORBIS_LOG_ERROR(__FUNCTION__, client->name, "sync call", _params.method);

  std::lock_guard sessionLock(session->mutex);
  auto server = session->server;

  if (server == nullptr) {
    return ErrorCode::INVAL;
  }

  {
    std::lock_guard serverLock(server->mutex);

    // ORBIS_LOG_TODO("IPMI: invokeSyncMethod", client->name, _params.method,
    //                _params.numInData, _params.unk, _params.numOutData,
    //                _params.pInData, _params.pOutData, _params.pResult,
    //                _params.flags);

    std::size_t inSize = 0;
    for (auto &data : std::span(_params.pInData, _params.numInData)) {
      inSize += data.size;
    }

    std::size_t outSize = 0;
    for (auto &data : std::span(_params.pOutData, _params.numOutData)) {
      outSize += data.size;
    }

    auto size = sizeof(MessageHeader) + inSize + outSize +
                _params.numInData * sizeof(uint32_t) +
                _params.numOutData * sizeof(uint32_t);

    kvector<std::byte> message(size);
    auto msg = new (message.data()) MessageHeader;
    msg->sessionImpl = session->sessionImpl;
    msg->pid = thread->tproc->pid;
    msg->methodId = _params.method;
    msg->numInData = _params.numInData;
    msg->numOutData = _params.numOutData;

    ORBIS_LOG_TODO("IPMI: sync call", client->name, _params.method,
                   thread->tproc->pid);
    thread->where();

    auto bufLoc = std::bit_cast<char *>(msg + 1);

    for (auto &data : std::span(_params.pInData, _params.numInData)) {
      *std::bit_cast<uint32_t *>(bufLoc) = data.size;
      bufLoc += sizeof(uint32_t);
      ORBIS_RET_ON_ERROR(ureadRaw(bufLoc, data.data, data.size));
      bufLoc += data.size;
    }

    for (auto &data : std::span(_params.pOutData, _params.numOutData)) {
      *std::bit_cast<uint32_t *>(bufLoc) = data.size;
      bufLoc += sizeof(uint32_t) + data.size;
    }

    uint type = 0x41;

    if (_params.numInData == 1 && _params.numOutData == 1 &&
        server->pid == thread->tproc->pid) {
      type |= 0x10;
    }

    if ((_params.flags & 1) == 0) {
      type |= 0x8000;
    }

    session->expectedOutput = _params.numOutData > 0;
    server->packets.push_back(
        {{.type = type, .clientKid = kid}, std::move(message)});
    server->receiveCv.notify_one(server->mutex);
  }

  while (session->messageResponses.empty()) {
    session->responseCv.wait(session->mutex);
  }

  auto response = std::move(session->messageResponses.front());
  session->messageResponses.pop_front();

  ORBIS_RET_ON_ERROR(uwrite(_params.pResult, response.errorCode));
  if (_params.numOutData > 0 && _params.pOutData->size < response.data.size()) {
    return ErrorCode::INVAL;
  }

  if (_params.numOutData && _params.pOutData->size) {
    ORBIS_RET_ON_ERROR(uwriteRaw(_params.pOutData->data, response.data.data(),
                                 response.data.size()));
    _params.pOutData->size = response.data.size();
  }

  ORBIS_LOG_TODO(__FUNCTION__, "sync message response", client->name, _params.method,
                 response.errorCode, response.data.size());
  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiClientConnect(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  struct IpmiClientConnectParams {
    ptr<void> arg0;
    ptr<void> arg1;
    ptr<sint> status;
    ptr<void> arg3;
  };

  static_assert(sizeof(IpmiClientConnectParams) == 0x20);

  if (paramsSz != sizeof(IpmiClientConnectParams)) {
    return ErrorCode::INVAL;
  }

  auto client = thread->tproc->ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  if (client->session != nullptr) {
    return ErrorCode::EXIST;
  }

  IpmiClientConnectParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiClientConnectParams>(params)));

  ORBIS_LOG_ERROR(__FUNCTION__, client->name, "connect");

  auto server = g_context.findIpmiServer(client->name);

  if (server == nullptr) {
    ORBIS_LOG_ERROR(__FUNCTION__, "waiting for server", client->name);

    while (server == nullptr) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      server = g_context.findIpmiServer(client->name);
    }
  }

  std::lock_guard clientLock(client->mutex);

  decltype(server->connectionRequests)::iterator requestIt;

  {
    std::lock_guard serverLock(server->mutex);

    for (auto &connReq : server->connectionRequests) {
      if (connReq.client == client) {
        return ErrorCode::EXIST;
      }
    }

    server->connectionRequests.push_front({
        .client = client,
        .clientTid = thread->tid,
        .clientPid = thread->tproc->pid,
    });

    requestIt = server->connectionRequests.begin();

    struct QueueStats {
      uint maxOutstanding;
      uint unk;
      ulong inDataSizeHardLimit;
      ulong outDataSizeHardLimit;
    };

    struct ConnectMessageHeader {
      uint32_t pid;
      uint32_t unk0;
      QueueStats sync;
      QueueStats async;
      uint numEventFlag;
      uint unk1;
      uint numMsgQueue;
      uint unk2;
      ulong msgQueueSize[32];
      ulong memorySize;
    };

    static_assert(sizeof(ConnectMessageHeader) == 0x150);

    struct ConnectFields {
      uint unk0;
      uint unk1;
    };

    kvector<std::byte> message{sizeof(ConnectMessageHeader) +
                               sizeof(ConnectFields)};
    auto header = new (message.data()) ConnectMessageHeader{};
    header->pid = thread->tproc->pid;

    server->packets.push_back(
        {{
             .userData = (ptr<void>)static_cast<ulong>(thread->tid),
             .type = 1,
             .clientKid = kid,
         },
         std::move(message)});

    server->receiveCv.notify_one(server->mutex);
  }

  ORBIS_LOG_ERROR(__FUNCTION__, client->name, "connect: packet sent");

  while (client->session == nullptr) {
    client->sessionCv.wait(client->mutex);
  }

  ORBIS_LOG_ERROR(__FUNCTION__, client->name, "connect: session created");
  ORBIS_RET_ON_ERROR(uwrite(_params.status, 0)); // TODO

  {
    std::lock_guard serverLock(server->mutex);
    server->connectionRequests.erase(requestIt);
  }
  return uwrite(result, 0u);
}

orbis::SysResult orbis::sysIpmiSessionGetUserData(Thread *thread,
                                                  ptr<uint> result, uint kid,
                                                  ptr<void> params,
                                                  uint64_t paramsSz) {
  struct IpmiGetUserDataParam {
    ptr<ptr<void>> data;
  };

  if (paramsSz != sizeof(IpmiGetUserDataParam)) {
    return ErrorCode::INVAL;
  }

  auto session =
      dynamic_cast<IpmiSession *>(thread->tproc->ipmiMap.get(kid).get());

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiGetUserDataParam _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiGetUserDataParam>(params)));
  ORBIS_RET_ON_ERROR(uwrite(_params.data, session->userData));
  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiServerGetName(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  struct IpmiGetServerNameParams {
    ptr<char> name;
  };

  if (paramsSz != sizeof(IpmiGetServerNameParams)) {
    return ErrorCode::INVAL;
  }

  auto server = thread->tproc->ipmiMap.get(kid).cast<IpmiServer>();

  if (server == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiGetServerNameParams _param;
  ORBIS_RET_ON_ERROR(uread(_param, ptr<IpmiGetServerNameParams>(params)));
  ORBIS_RET_ON_ERROR(
      uwriteRaw(_param.name, server->name.c_str(), server->name.size() + 1));

  return uwrite<uint>(result, 0);
}
