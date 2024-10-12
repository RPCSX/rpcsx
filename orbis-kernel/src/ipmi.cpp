#include "ipmi.hpp"
#include "KernelContext.hpp"
#include "thread/Process.hpp"
#include "utils/Logs.hpp"
#include <chrono>
#include <span>
#include <sys/mman.h>

orbis::ErrorCode orbis::ipmiCreateClient(Process *proc, void *clientImpl,
                                         const char *name,
                                         const IpmiCreateClientConfig &config,
                                         Ref<IpmiClient> &result) {
  auto client = knew<IpmiClient>(name);
  if (client == nullptr) {
    return ErrorCode::NOMEM;
  }

  client->clientImpl = clientImpl;
  client->name = name;
  client->process = proc;
  client->userData = config.userData;
  client->eventFlags.resize(32);
  client->messageQueues.resize(32);
  result = client;
  return {};
}

orbis::ErrorCode orbis::ipmiCreateServer(Process *proc, void *serverImpl,
                                         const char *name,
                                         const IpmiCreateServerConfig &config,
                                         Ref<IpmiServer> &result) {
  auto [server, errorCode] = g_context.createIpmiServer(name);
  ORBIS_RET_ON_ERROR(errorCode);

  server->serverImpl = serverImpl;
  server->userData = config.userData;
  server->eventHandler = config.eventHandler;
  server->pid = proc->pid;
  result = server;
  return {};
}

orbis::ErrorCode orbis::ipmiCreateSession(Thread *thread, void *sessionImpl,
                                          ptr<void> userData,
                                          Ref<IpmiSession> &result) {
  std::unique_lock ipmiMapLock(g_context.ipmiMap.mutex);

  for (auto [kid, obj] : g_context.ipmiMap) {
    auto server = dynamic_cast<IpmiServer *>(obj);
    if (server == nullptr) {
      continue;
    }

    std::lock_guard serverLock(server->mutex);
    for (auto &conReq : server->connectionRequests) {
      if (conReq.serverTid != thread->tid ||
          conReq.client->session != nullptr ||
          conReq.client->name != server->name) {
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
      conReq.client->sessionCv.notify_one(conReq.client->mutex);

      return {};
    }
  }

  ORBIS_LOG_ERROR(__FUNCTION__, ": connection request not found");
  return ErrorCode::INVAL;
}

orbis::SysResult orbis::sysIpmiCreateClient(Thread *thread, ptr<uint> result,
                                            ptr<void> params,
                                            uint64_t paramsSz) {
  if (paramsSz != sizeof(IpmiCreateClientParams)) {
    return ErrorCode::INVAL;
  }

  IpmiCreateClientParams _params;
  IpmiCreateClientConfig _config;
  char _name[25];
  Ref<IpmiClient> client;

  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiCreateClientParams>(params)));
  ORBIS_RET_ON_ERROR(uread(_config, _params.config));
  ORBIS_RET_ON_ERROR(ureadString(_name, sizeof(_name), _params.name));
  ORBIS_RET_ON_ERROR(ipmiCreateClient(thread->tproc, _params.clientImpl, _name,
                                      _config, client));

  auto kid = g_context.ipmiMap.insert(std::move(client));

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
  ORBIS_RET_ON_ERROR(ipmiCreateServer(thread->tproc, _params.serverImpl, _name,
                                      _config, server));
  auto kid = g_context.ipmiMap.insert(std::move(server));

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

  auto kid = g_context.ipmiMap.insert(std::move(session));

  if (kid == -1) {
    return ErrorCode::MFILE;
  }

  ORBIS_LOG_ERROR(__FUNCTION__, kid);
  return uwrite<uint>(result, kid);
}

orbis::SysResult orbis::sysIpmiDestroyClient(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  ORBIS_LOG_TODO(__FUNCTION__);
  return uwrite<uint>(result, 0);
}
orbis::SysResult orbis::sysIpmiDestroyServer(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  ORBIS_LOG_TODO(__FUNCTION__);
  return uwrite<uint>(result, 0);
}
orbis::SysResult orbis::sysIpmiDestroySession(Thread *thread, ptr<uint> result,
                                              uint kid, ptr<void> params,
                                              uint64_t paramsSz) {
  ORBIS_LOG_TODO(__FUNCTION__);
  return uwrite<uint>(result, 0);
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

  if (paramsSz != sizeof(IpmiServerReceivePacketParams)) {
    return orbis::ErrorCode::INVAL;
  }

  IpmiServerReceivePacketParams _params;

  ORBIS_RET_ON_ERROR(
      uread(_params, ptr<IpmiServerReceivePacketParams>(params)));

  auto server = g_context.ipmiMap.get(kid).cast<IpmiServer>();

  if (server == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiServer::Packet _packet;

  {
    std::lock_guard lock(server->mutex);
    while (server->packets.empty()) {
      server->receiveCv.wait(server->mutex);
    }

    _packet = std::move(server->packets.front());
    server->packets.pop_front();
  }

  if (_packet.info.type == 0x1) {
    // on connection packet

    for (auto &conn : server->connectionRequests) {
      if (conn.clientTid == _packet.info.inputSize) {
        conn.serverTid = thread->tid;
        _packet.info.inputSize = 0;
        break;
      }
    }
  } else if ((_packet.info.type & ~(0x10 | 0x8000)) == 0x41) {
    auto syncMessage = (IpmiSyncMessageHeader *)_packet.message.data();
    ORBIS_LOG_ERROR(__FUNCTION__, server->name, syncMessage->methodId,
                    syncMessage->numInData, syncMessage->numOutData,
                    syncMessage->pid);
  } else if ((_packet.info.type & ~0x10) == 0x43) {
    auto asyncMessage = (IpmiAsyncMessageHeader *)_packet.message.data();
    ORBIS_LOG_ERROR(__FUNCTION__, server->name, asyncMessage->methodId,
                    asyncMessage->numInData, asyncMessage->pid);
  }

  if (_params.bufferSize < _packet.message.size()) {
    ORBIS_LOG_ERROR(__FUNCTION__, "too small buffer", _params.bufferSize,
                    _packet.message.size());
    return ErrorCode::INVAL;
  }

  server->tidToClientTid[thread->tid] = _packet.clientTid;

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
    return ErrorCode::INVAL;
  }

  ORBIS_LOG_NOTICE(__FUNCTION__, kid);

  auto ipmiObject = g_context.ipmiMap.get(kid);
  if (ipmiObject == nullptr) {
    return ErrorCode::INVAL;
  }

  Ref<IpmiClient> client;
  if (auto result = ipmiObject.cast<IpmiSession>()) {
    client = result->client;
  } else if (auto result = ipmiObject.cast<IpmiClient>()) {
    client = result;
  } else if (auto result = ipmiObject.cast<IpmiServer>()) {
    for (auto &request : result->connectionRequests) {
      if (request.serverTid == thread->tid) {
        client = request.client;
        break;
      }
    }
  }

  if (client == nullptr) {
    ORBIS_LOG_FATAL(__FUNCTION__);
    std::abort();
  }

  sint status;
  ORBIS_RET_ON_ERROR(uread(status, ptr<sint>(params)));

  ORBIS_LOG_NOTICE(__FUNCTION__, kid, status);
  std::lock_guard lock(client->mutex);
  client->connectionStatus = status;
  client->connectCv.notify_all(client->mutex);
  return uwrite(result, 0u);
}
orbis::SysResult orbis::sysIpmiSessionRespondSync(Thread *thread,
                                                  ptr<uint> result, uint kid,
                                                  ptr<void> params,
                                                  uint64_t paramsSz) {
  struct IpmiRespondParams {
    sint errorCode;
    uint32_t bufferCount;
    ptr<IpmiBufferInfo> buffers;
    uint32_t flags;
    uint32_t padding;
  };

  static_assert(sizeof(IpmiRespondParams) == 0x18);
  if (paramsSz != sizeof(IpmiRespondParams)) {
    return ErrorCode::INVAL;
  }

  auto session = g_context.ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiRespondParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiRespondParams>(params)));

  kvector<kvector<std::byte>> buffers;

  // if ((_params.flags & 1) || _params.bufferCount != 1) {
  auto count = _params.bufferCount;
  buffers.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    IpmiBufferInfo _buffer;
    ORBIS_RET_ON_ERROR(uread(_buffer, _params.buffers + i));

    auto &bufferData = buffers.emplace_back();
    bufferData.resize(_buffer.size);
    ORBIS_RET_ON_ERROR(ureadRaw(bufferData.data(), _buffer.data, _buffer.size));
  }
  // }

  std::lock_guard lock(session->mutex);

  std::uint32_t clientTid;
  {
    std::lock_guard serverLock(session->server->mutex);
    clientTid = session->server->tidToClientTid.at(thread->tid);
  }

  ORBIS_LOG_ERROR(__FUNCTION__, session->client->name, _params.errorCode);

  if (_params.errorCode != 0) {
    ORBIS_LOG_ERROR(__FUNCTION__, session->client->name, _params.errorCode);
    thread->where();

    // HACK: completely broken audio support should not be visible
    if (session->client->name == "SceSysAudioSystemIpc" &&
        _params.errorCode == -1) {
      _params.errorCode = 0;
    }
  }

  session->syncResponses.push_front({
      .errorCode = _params.errorCode,
      .callerTid = clientTid,
      .data = std::move(buffers),
  });

  session->responseCv.notify_all(session->mutex);
  return uwrite(result, 0u);
}
orbis::SysResult orbis::sysIpmiClientInvokeAsyncMethod(Thread *thread,
                                                       ptr<uint> result,
                                                       uint kid,
                                                       ptr<void> params,
                                                       uint64_t paramsSz) {
  struct IpmiAsyncCallParams {
    uint32_t method;
    uint32_t evfIndex;
    uint64_t evfValue;
    uint32_t numInData;
    uint32_t padding1;
    ptr<IpmiDataInfo> pInData;
    ptr<sint> pResult;
    uint32_t flags;
  };

  static_assert(sizeof(IpmiAsyncCallParams) == 0x30);

  if (paramsSz != sizeof(IpmiAsyncCallParams)) {
    return ErrorCode::INVAL;
  }

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiAsyncCallParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, (ptr<IpmiAsyncCallParams>)params));

  if (_params.flags > 1) {
    return ErrorCode::INVAL;
  }

  std::lock_guard clientLock(client->mutex);
  auto session = client->session;

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  std::lock_guard sessionLock(session->mutex);
  auto server = session->server;

  if (server == nullptr) {
    return ErrorCode::INVAL;
  }

  {
    std::lock_guard serverLock(server->mutex);

    std::size_t inSize = 0;
    for (auto &data : std::span(_params.pInData, _params.numInData)) {
      inSize += data.size;
    }

    auto size = sizeof(IpmiAsyncMessageHeader) + inSize +
                _params.numInData * sizeof(uint32_t);
    kvector<std::byte> message(size);
    auto msg = new (message.data()) IpmiAsyncMessageHeader;
    msg->sessionImpl = session->sessionImpl;
    msg->pid = thread->tproc->pid;
    msg->methodId = _params.method;
    msg->numInData = _params.numInData;

    auto bufLoc = std::bit_cast<char *>(msg + 1);

    for (auto &data : std::span(_params.pInData, _params.numInData)) {
      *std::bit_cast<uint32_t *>(bufLoc) = data.size;
      bufLoc += sizeof(uint32_t);
      ORBIS_RET_ON_ERROR(ureadRaw(bufLoc, data.data, data.size));
      bufLoc += data.size;
    }

    uint type = 0x43;

    if ((_params.flags & 1) == 0) {
      type |= 0x10;
    }

    server->packets.push_back(
        {{.type = type, .clientKid = kid}, 0, session, std::move(message)});
    server->receiveCv.notify_one(server->mutex);
  }

  if (_params.evfIndex != -1 && _params.evfValue != 0) {
    client->eventFlags[_params.evfIndex].set(_params.evfValue);
  }

  ORBIS_RET_ON_ERROR(uwrite(_params.pResult, 0));
  return uwrite(result, 0u);
}

orbis::SysResult orbis::sysImpiSessionRespondAsync(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  struct IpmiAsyncRespondParams {
    uint method;
    uint pid;
    sint result;
    uint32_t numOutData;
    ptr<IpmiDataInfo> pOutData;
    uint32_t unk2; // == 1
  };

  static_assert(sizeof(IpmiAsyncRespondParams) == 0x20);

  if (paramsSz != sizeof(IpmiAsyncRespondParams)) {
    return ErrorCode::INVAL;
  }

  auto session = g_context.ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  auto client = session->client;

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiAsyncRespondParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, (ptr<IpmiAsyncRespondParams>)params));

  kvector<kvector<std::byte>> outData;
  outData.reserve(_params.numOutData);
  for (auto data : std::span(_params.pOutData, _params.numOutData)) {
    auto &elem = outData.emplace_back();
    elem.resize(data.size);
    ORBIS_RET_ON_ERROR(ureadRaw(elem.data(), data.data, data.size));
  }

  {
    std::lock_guard clientLock(client->mutex);
    client->asyncResponses.push_back({
        .methodId = _params.method,
        .errorCode = _params.result,
        .data = std::move(outData),
    });
  }

  client->asyncResponseCv.notify_all(client->mutex);
  return uwrite(result, 0u);
}

orbis::SysResult orbis::sysIpmiClientTryGetResult(Thread *thread,
                                                  ptr<uint> result, uint kid,
                                                  ptr<void> params,
                                                  uint64_t paramsSz) {
  struct IpmiTryGetResultParams {
    uint32_t method;
    uint32_t unk;
    ptr<sint> pResult;
    uint32_t numOutData;
    uint32_t padding;
    ptr<IpmiBufferInfo> pOutData;
    uint64_t padding2;
  };

  static_assert(sizeof(IpmiTryGetResultParams) == 0x28);

  if (paramsSz != sizeof(IpmiTryGetResultParams)) {
    return ErrorCode::INVAL;
  }

  IpmiTryGetResultParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, (ptr<IpmiTryGetResultParams>)params));

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  while (true) {
    std::lock_guard clientLock(client->mutex);

    for (auto it = client->asyncResponses.begin();
         it != client->asyncResponses.end(); ++it) {
      if (it->methodId != _params.method) {
        continue;
      }

      auto response = std::move(*it);
      client->asyncResponses.erase(it);

      ORBIS_RET_ON_ERROR(uwrite(_params.pResult, it->errorCode));

      if (response.data.size() != _params.numOutData) {
        ORBIS_LOG_ERROR(__FUNCTION__, "responses count mismatch",
                        response.data.size(), _params.numOutData);
      }

      for (std::size_t i = 0; i < response.data.size(); ++i) {
        if (response.data.size() > _params.numOutData) {
          ORBIS_LOG_ERROR(__FUNCTION__, "too many responses",
                          response.data.size(), _params.numOutData);
          break;
        }

        IpmiBufferInfo _outData;
        ORBIS_RET_ON_ERROR(uread(_outData, _params.pOutData + i));

        auto &data = response.data[i];

        if (_outData.capacity < data.size()) {
          ORBIS_LOG_ERROR(__FUNCTION__, "too big response", _outData.capacity,
                          data.size());
          continue;
        }

        _outData.size = data.size();
        ORBIS_RET_ON_ERROR(uwriteRaw(_outData.data, data.data(), data.size()));
        ORBIS_RET_ON_ERROR(uwrite(_params.pOutData + i, _outData));
      }

      return uwrite(result, 0u);
    }

    client->asyncResponseCv.wait(client->mutex);
  }

  // return uwrite(result, 0x80020000 + static_cast<int>(ErrorCode::AGAIN));
}

orbis::SysResult orbis::sysIpmiClientGetMessage(Thread *thread,
                                                ptr<uint> result, uint kid,
                                                ptr<void> params,
                                                uint64_t paramsSz) {
  struct SceIpmiClientGetArgs {
    uint32_t queueIndex;
    uint32_t padding;
    ptr<std::byte> message;
    ptr<uint64_t> pSize;
    uint64_t maxSize;
    ptr<uint> pTimeout;
  };

  static_assert(sizeof(SceIpmiClientGetArgs) == 0x28);

  if (paramsSz != sizeof(SceIpmiClientGetArgs)) {
    return ErrorCode::INVAL;
  }

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  SceIpmiClientGetArgs _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<SceIpmiClientGetArgs>(params)));

  std::lock_guard lock(client->mutex);

  if (_params.queueIndex >= client->messageQueues.size()) {
    return ErrorCode::INVAL;
  }

  auto &queue = client->messageQueues[_params.queueIndex];

  using clock = std::chrono::high_resolution_clock;

  clock::time_point timeoutPoint = clock::time_point::max();
  if (_params.pTimeout != nullptr) {
    std::uint32_t timeout{};
    ORBIS_RET_ON_ERROR(uread(timeout, _params.pTimeout));
    timeoutPoint = clock::now() + std::chrono::microseconds(timeout);
  }

  if (queue.messages.empty()) {
    if (timeoutPoint != clock::time_point::max()) {
      while (true) {
        auto now = clock::now();
        if (now >= timeoutPoint) {
          ORBIS_RET_ON_ERROR(uwrite(_params.pTimeout, 0u));
          return uwrite<uint>(
              result, 0x80020000 + static_cast<int>(ErrorCode::TIMEDOUT));
        }

        auto waitTime = std::chrono::duration_cast<std::chrono::microseconds>(
            timeoutPoint - now);
        queue.messageCv.wait(client->mutex, waitTime.count());

        if (!queue.messages.empty()) {
          now = clock::now();

          if (now >= timeoutPoint) {
            ORBIS_RET_ON_ERROR(uwrite(_params.pTimeout, 0u));
          } else {
            std::uint32_t restTime =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    timeoutPoint - now)
                    .count();
            ORBIS_RET_ON_ERROR(uwrite(_params.pTimeout, restTime));
          }

          break;
        }
      }
    } else {
      while (queue.messages.empty()) {
        queue.messageCv.wait(client->mutex);
      }
    }
  }

  auto &message = queue.messages.front();

  if (_params.maxSize < message.size()) {
    ORBIS_LOG_ERROR(__FUNCTION__, "too small buffer");
    return uwrite<uint>(result,
                        0x80020000 + static_cast<int>(ErrorCode::INVAL));
  }

  ORBIS_RET_ON_ERROR(uwrite(_params.pSize, message.size()));
  ORBIS_RET_ON_ERROR(
      uwriteRaw(_params.message, message.data(), message.size()));
  queue.messages.pop_front();
  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiClientTryGetMessage(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  struct SceIpmiClientTryGetArgs {
    uint32_t queueIndex;
    uint32_t padding;
    ptr<std::byte> message;
    ptr<uint64_t> pSize;
    uint64_t maxSize;
  };

  static_assert(sizeof(SceIpmiClientTryGetArgs) == 0x20);

  if (paramsSz != sizeof(SceIpmiClientTryGetArgs)) {
    return ErrorCode::INVAL;
  }

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  SceIpmiClientTryGetArgs _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<SceIpmiClientTryGetArgs>(params)));

  std::lock_guard lock(client->mutex);

  if (_params.queueIndex >= client->messageQueues.size()) {
    return ErrorCode::INVAL;
  }

  auto &queue = client->messageQueues[_params.queueIndex];

  if (queue.messages.empty()) {
    return uwrite<uint>(result,
                        0x80020000 + static_cast<int>(ErrorCode::AGAIN));
  }

  auto &message = queue.messages.front();

  if (_params.maxSize < message.size()) {
    ORBIS_LOG_ERROR(__FUNCTION__, "too small buffer");
    return uwrite<uint>(result,
                        0x80020000 + static_cast<int>(ErrorCode::INVAL));
  }

  ORBIS_RET_ON_ERROR(uwrite(_params.pSize, message.size()));
  ORBIS_RET_ON_ERROR(
      uwriteRaw(_params.message, message.data(), message.size()));
  queue.messages.pop_front();
  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiSessionTrySendMessage(Thread *thread,
                                                     ptr<uint> result, uint kid,
                                                     ptr<void> params,
                                                     uint64_t paramsSz) {
  struct SceIpmiClientTrySendArgs {
    uint32_t queueIndex;
    uint32_t padding;
    ptr<std::byte> message;
    uint64_t size;
  };

  static_assert(sizeof(SceIpmiClientTrySendArgs) == 0x18);

  if (paramsSz != sizeof(SceIpmiClientTrySendArgs)) {
    return ErrorCode::INVAL;
  }

  auto session = g_context.ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  SceIpmiClientTrySendArgs _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<SceIpmiClientTrySendArgs>(params)));

  std::lock_guard lock(session->mutex);

  if (session->client == nullptr) {
    return ErrorCode::INVAL;
  }

  auto client = session->client;
  std::lock_guard lockClient(client->mutex);

  if (_params.queueIndex >= client->messageQueues.size()) {
    return ErrorCode::INVAL;
  }

  auto &queue = client->messageQueues[_params.queueIndex];

  auto &message = queue.messages.emplace_back();
  message.resize(_params.size);
  ORBIS_RET_ON_ERROR(ureadRaw(message.data(), _params.message, _params.size));
  queue.messageCv.notify_all(client->mutex);
  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiClientDisconnect(Thread *thread,
                                                ptr<uint> result, uint kid,
                                                ptr<void> params,
                                                uint64_t paramsSz) {
  struct SceIpmiClientDisconnectArgs {
    ptr<sint> status;
  };

  if (paramsSz != sizeof(SceIpmiClientDisconnectArgs)) {
    return ErrorCode::INVAL;
  }

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  SceIpmiClientDisconnectArgs _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<SceIpmiClientDisconnectArgs>(params)));

  ORBIS_LOG_ERROR(__FUNCTION__, client->name, _params.status);
  ORBIS_RET_ON_ERROR(uwrite(_params.status, 0));
  return uwrite<uint>(result, 0);
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

  auto session = g_context.ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiGetClientPidParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiGetClientPidParams>(params)));
  ORBIS_RET_ON_ERROR(
      uwrite<uint32_t>(_params.pid, session->client->process->pid));
  return uwrite<uint>(result, 0);
}

orbis::SysResult
orbis::sysIpmiClientInvokeSyncMethod(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz) {
  if (paramsSz != sizeof(IpmiSyncCallParams)) {
    return ErrorCode::INVAL;
  }

  IpmiSyncCallParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, (ptr<IpmiSyncCallParams>)params));

  if (_params.flags > 1) {
    return ErrorCode::INVAL;
  }

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  auto session = client->session;

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  std::lock_guard sessionLock(session->mutex);
  auto server = session->server;

  if (server == nullptr) {
    return ErrorCode::INVAL;
  }

  {
    std::lock_guard serverLock(server->mutex);

    std::size_t inSize = 0;
    for (auto &data : std::span(_params.pInData, _params.numInData)) {
      inSize += data.size;
    }

    auto headerSize = sizeof(IpmiSyncMessageHeader) + inSize +
                      _params.numInData * sizeof(uint32_t);
    auto size = headerSize + _params.numOutData * sizeof(uint);

    kvector<std::byte> message(size);
    auto msg = new (message.data()) IpmiSyncMessageHeader;
    msg->sessionImpl = session->sessionImpl;
    msg->pid = thread->tproc->pid;
    msg->methodId = _params.method;
    msg->numInData = _params.numInData;
    msg->numOutData = _params.numOutData;

    auto bufLoc = std::bit_cast<char *>(msg + 1);

    for (auto &data : std::span(_params.pInData, _params.numInData)) {
      *std::bit_cast<uint32_t *>(bufLoc) = data.size;
      bufLoc += sizeof(uint32_t);
      ORBIS_RET_ON_ERROR(ureadRaw(bufLoc, data.data, data.size));
      bufLoc += data.size;
    }

    for (auto &data : std::span(_params.pOutData, _params.numOutData)) {
      *std::bit_cast<uint32_t *>(bufLoc) = data.capacity;
      bufLoc += sizeof(uint32_t);
    }

    uint type = 0x41;

    if ((_params.flags & 1) == 0) {
      type |= 0x10;
    }

    if (server->pid == thread->tproc->pid) {
      type |= 0x8000;
    }

    server->packets.push_back(
        {{.inputSize = headerSize, .type = type, .clientKid = kid},
         thread->tid,
         session,
         std::move(message)});
    server->receiveCv.notify_one(server->mutex);
  }

  IpmiSession::SyncResponse response;

  while (true) {
    session->responseCv.wait(session->mutex);

    bool found = false;
    for (auto it = session->syncResponses.begin();
         it != session->syncResponses.end(); ++it) {
      if (it->callerTid != thread->tid) {
        continue;
      }

      response = std::move(*it);
      session->syncResponses.erase(it);
      found = true;
      break;
    }
    if (found) {
      break;
    }
  }

  if (response.errorCode != 0) {
    thread->where();
  }

  ORBIS_RET_ON_ERROR(uwrite(_params.pResult, response.errorCode));

  if (response.data.size() != _params.numOutData) {
    ORBIS_LOG_ERROR(__FUNCTION__, "responses amount mismatch",
                    response.data.size(), _params.numOutData);
  }

  for (std::size_t i = 0; i < response.data.size(); ++i) {
    if (response.data.size() > _params.numOutData) {
      ORBIS_LOG_ERROR(__FUNCTION__, "too many responses", response.data.size(),
                      _params.numOutData);
      break;
    }

    IpmiBufferInfo _outData;
    ORBIS_RET_ON_ERROR(uread(_outData, _params.pOutData + i));

    auto &data = response.data[i];

    if (_outData.capacity < data.size()) {
      ORBIS_LOG_ERROR(__FUNCTION__, "too big response", _outData.capacity,
                      data.size());
      continue;
    }

    ORBIS_LOG_ERROR(__FUNCTION__, i, _outData.data, _outData.capacity,
                    data.size());

    _outData.size = data.size();
    ORBIS_RET_ON_ERROR(uwriteRaw(_outData.data, data.data(), data.size()));
    ORBIS_RET_ON_ERROR(uwrite(_params.pOutData + i, _outData));
  }

  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiClientConnect(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  if (paramsSz != sizeof(IpmiClientConnectParams)) {
    return ErrorCode::INVAL;
  }

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  if (client->session != nullptr) {
    return ErrorCode::EXIST;
  }

  IpmiClientConnectParams _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiClientConnectParams>(params)));

  auto server = g_context.findIpmiServer(client->name);

  if (server == nullptr) {
    return SysResult::notAnError(ErrorCode::NOENT);
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
      uint32_t clientPid;
      uint32_t clientKid;
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

    kvector<std::byte> message{
        sizeof(ConnectMessageHeader) + sizeof(uint) +
        std::max<std::size_t>(_params.userDataLen, 0x10)};
    auto header = new (message.data()) ConnectMessageHeader{};
    header->clientPid = thread->tproc->pid;
    header->clientKid = kid;

    header->sync.maxOutstanding = 1;
    header->sync.inDataSizeHardLimit = 0x10000;
    header->sync.outDataSizeHardLimit = 0x10000;
    header->async.maxOutstanding = 8;
    header->async.inDataSizeHardLimit = 0x10000;
    header->async.outDataSizeHardLimit = 0x10000;

    header->numEventFlag = client->eventFlags.size();
    header->numMsgQueue = client->messageQueues.size();

    for (auto &size : header->msgQueueSize) {
      size = 0x10000;
    }

    if (_params.userDataLen != 0) {
      auto bufLoc = std::bit_cast<char *>(header + 1);
      *std::bit_cast<uint *>(bufLoc) = _params.userDataLen;
      ORBIS_RET_ON_ERROR(ureadRaw(bufLoc + sizeof(uint), _params.userData,
                                  _params.userDataLen));
    }

    server->packets.push_back({{
                                   .inputSize = static_cast<ulong>(thread->tid),
                                   .type = 1,
                                   .clientKid = kid,
                               },
                               0,
                               nullptr,
                               std::move(message)});
    server->receiveCv.notify_one(server->mutex);
  }

  while (client->session == nullptr) {
    client->sessionCv.wait(client->mutex);
  }

  while (!client->connectionStatus) {
    client->connectCv.wait(client->mutex);
  }

  ORBIS_RET_ON_ERROR(uwrite(_params.status, *client->connectionStatus));

  {
    std::lock_guard serverLock(server->mutex);
    server->connectionRequests.erase(requestIt);
  }

  return uwrite(result, 0u);
}

orbis::SysResult orbis::sysIpmiSessionGetClientAppId(Thread *thread,
                                                     ptr<uint> result, uint kid,
                                                     ptr<void> params,
                                                     uint64_t paramsSz) {
  struct IpmiGetUserDataParam {
    ptr<uint> data;
  };

  if (paramsSz != sizeof(IpmiGetUserDataParam)) {
    return ErrorCode::INVAL;
  }

  auto session = g_context.ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiGetUserDataParam _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiGetUserDataParam>(params)));
  ORBIS_RET_ON_ERROR(
      uwrite(_params.data, session->client->process->appInfo.appId));
  return uwrite<uint>(result, 0);
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

  auto session = g_context.ipmiMap.get(kid).cast<IpmiSession>();

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

  auto server = g_context.ipmiMap.get(kid).cast<IpmiServer>();

  if (server == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiGetServerNameParams _param;
  ORBIS_RET_ON_ERROR(uread(_param, ptr<IpmiGetServerNameParams>(params)));
  ORBIS_RET_ON_ERROR(
      uwriteRaw(_param.name, server->name.c_str(), server->name.size() + 1));

  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiClientGetName(Thread *thread, ptr<uint> result,
                                             uint kid, ptr<void> params,
                                             uint64_t paramsSz) {
  struct IpmiGetClientNameParams {
    ptr<char> name;
  };

  if (paramsSz != sizeof(IpmiGetClientNameParams)) {
    return ErrorCode::INVAL;
  }

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  IpmiGetClientNameParams _param;
  ORBIS_RET_ON_ERROR(uread(_param, ptr<IpmiGetClientNameParams>(params)));
  ORBIS_RET_ON_ERROR(
      uwriteRaw(_param.name, client->name.c_str(), client->name.size() + 1));

  return uwrite<uint>(result, 0);
}

orbis::SysResult orbis::sysIpmiClientWaitEventFlag(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  struct IpmiWaitEventFlagParam {
    uint32_t index;
    uint32_t padding0;
    uint64_t patternSet;
    uint32_t mode;
    uint32_t padding1;
    ptr<uint64_t> pPatternSet;
    ptr<uint32_t> pTimeout;
  };

  static_assert(sizeof(IpmiWaitEventFlagParam) == 0x28);

  if (paramsSz != sizeof(IpmiWaitEventFlagParam)) {
    return ErrorCode::INVAL;
  }

  IpmiWaitEventFlagParam _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiWaitEventFlagParam>(params)));

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  if (client->eventFlags.size() <= _params.index) {
    return ErrorCode::INVAL;
  }

  std::uint32_t resultTimeout{};

  if (_params.pTimeout != nullptr) {
    ORBIS_RET_ON_ERROR(uread(resultTimeout, _params.pTimeout));
  }

  auto &evf = client->eventFlags[_params.index];
  auto waitResult = evf.wait(thread, _params.mode, _params.patternSet,
                             _params.pTimeout != 0 ? &resultTimeout : nullptr);

  if (_params.pPatternSet != nullptr) {
    ORBIS_RET_ON_ERROR(uwrite(_params.pPatternSet, thread->evfResultPattern));
  }

  ORBIS_RET_ON_ERROR(uwrite(result, 0u));
  if (_params.pTimeout != nullptr) {
    ORBIS_RET_ON_ERROR(uwrite(_params.pTimeout, resultTimeout));
  }
  if (waitResult == ErrorCode::TIMEDOUT) {
    return SysResult::notAnError(ErrorCode::TIMEDOUT);
  }
  return waitResult;
}

orbis::SysResult orbis::sysIpmiClientPollEventFlag(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  struct IpmiPollEventFlagParam {
    uint32_t index;
    uint32_t padding0;
    uint64_t patternSet;
    uint32_t mode;
    uint32_t padding1;
    ptr<uint64_t> pPatternSet;
  };

  static_assert(sizeof(IpmiPollEventFlagParam) == 0x20);

  if (paramsSz != sizeof(IpmiPollEventFlagParam)) {
    return ErrorCode::INVAL;
  }

  IpmiPollEventFlagParam _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiPollEventFlagParam>(params)));

  auto client = g_context.ipmiMap.get(kid).cast<IpmiClient>();

  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  if (client->eventFlags.size() <= _params.index) {
    return ErrorCode::INVAL;
  }

  uint64_t patternSet;
  ORBIS_RET_ON_ERROR(uread(patternSet, _params.pPatternSet));
  auto &evf = client->eventFlags[_params.index];
  auto waitResult = evf.tryWait(thread, _params.mode, patternSet);

  ORBIS_RET_ON_ERROR(uread(patternSet, _params.pPatternSet));
  ORBIS_RET_ON_ERROR(uwrite(_params.pPatternSet, thread->evfResultPattern));
  ORBIS_RET_ON_ERROR(uwrite(result, 0u));
  return SysResult::notAnError(waitResult);
}

orbis::SysResult orbis::sysIpmiSessionSetEventFlag(Thread *thread,
                                                   ptr<uint> result, uint kid,
                                                   ptr<void> params,
                                                   uint64_t paramsSz) {
  struct IpmiSetEventFlagParam {
    uint32_t index;
    uint32_t padding;
    uint64_t patternSet;
  };

  static_assert(sizeof(IpmiSetEventFlagParam) == 0x10);

  if (paramsSz != sizeof(IpmiSetEventFlagParam)) {
    return ErrorCode::INVAL;
  }

  IpmiSetEventFlagParam _params;
  ORBIS_RET_ON_ERROR(uread(_params, ptr<IpmiSetEventFlagParam>(params)));

  auto session = g_context.ipmiMap.get(kid).cast<IpmiSession>();

  if (session == nullptr) {
    return ErrorCode::INVAL;
  }

  auto client = session->client;
  if (client == nullptr) {
    return ErrorCode::INVAL;
  }

  if (client->eventFlags.size() <= _params.index) {
    return ErrorCode::INVAL;
  }

  auto &evf = client->eventFlags[_params.index];
  evf.set(_params.patternSet);
  return uwrite<uint>(result, 0);
}
