#pragma once

#include "KernelAllocator.hpp"
#include "evf.hpp"
#include "orbis-config.hpp"
#include "orbis/utils/SharedCV.hpp"
#include "orbis/utils/SharedMutex.hpp"
#include "utils/Rc.hpp"
#include <list>
#include <span>

namespace orbis {
struct IpmiSession;
struct IpmiClient;
struct Thread;

struct IpmiServer : RcBase {
  struct IpmiPacketInfo {
    ptr<void> userData;
    uint type;
    uint clientKid;
    ptr<void> eventHandler;
  };

  static_assert(sizeof(IpmiPacketInfo) == 0x18);

  struct Packet {
    IpmiPacketInfo info;
    kvector<std::byte> message;
  };

  struct ConnectionRequest {
    Ref<IpmiClient> client;
    slong clientTid{};
    slong clientPid{};
    slong serverTid{};
  };

  kstring name;
  ptr<void> serverImpl;
  ptr<void> eventHandler;
  ptr<void> userData;
  shared_mutex mutex;
  shared_cv receiveCv;
  sint pid;
  kdeque<Packet> packets;
  std::list<ConnectionRequest, kallocator<ConnectionRequest>>
      connectionRequests;

  explicit IpmiServer(kstring name) : name(std::move(name)) {}
};

struct IpmiClient : RcBase {
  kstring name;
  ptr<void> clientImpl;
  ptr<void> userData;
  Ref<IpmiSession> session;
  shared_mutex mutex;
  shared_cv sessionCv;
  sint pid;

  explicit IpmiClient(kstring name) : name(std::move(name)) {}
};

struct IpmiSession : RcBase {
  struct MessageResponse {
    sint errorCode;
    kvector<std::byte> data;
  };

  ptr<void> sessionImpl;
  ptr<void> userData;
  Ref<IpmiClient> client;
  Ref<IpmiServer> server;
  shared_mutex mutex;
  shared_cv responseCv;
  kdeque<MessageResponse> messageResponses;
  EventFlag evf{0, 0};
  shared_cv connectCv;
  bool expectedOutput = false; // TODO: verify
  bool connected = false;      // TODO: implement more states
  sint connectionStatus{0};
};

struct IpmiCreateServerConfig {
  orbis::uint64_t size;
  orbis::uint32_t unk1;
  orbis::uint32_t unk2;
  orbis::uint32_t unk3;
  orbis::uint32_t unk4;
  orbis::uint32_t enableMultipleServerThreads;
  orbis::uint32_t unk5;
  orbis::uint64_t unk6;
  orbis::ptr<void> userData;
  orbis::ptr<void> eventHandler;
};

static_assert(sizeof(IpmiCreateServerConfig) == 0x38);

ErrorCode ipmiCreateClient(Thread *thread, void *clientImpl, const char *name,
                           void *config, Ref<IpmiClient> &result);
ErrorCode ipmiCreateServer(Thread *thread, void *serverImpl, const char *name,
                           const IpmiCreateServerConfig &config,
                           Ref<IpmiServer> &result);
ErrorCode ipmiCreateSession(Thread *thread, void *sessionImpl,
                            ptr<void> userData, Ref<IpmiSession> &result);

SysResult sysIpmiCreateClient(Thread *thread, ptr<uint> result,
                              ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiCreateServer(Thread *thread, ptr<uint> result,
                              ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiCreateSession(Thread *thread, ptr<uint> result,
                               ptr<void> params, uint64_t paramsSz);

SysResult sysIpmiDestroyClient(Thread *thread, ptr<uint> result, uint kid,
                               ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiDestroyServer(Thread *thread, ptr<uint> result, uint kid,
                               ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiDestroySession(Thread *thread, ptr<uint> result, uint kid,
                                ptr<void> params, uint64_t paramsSz);

SysResult sysIpmiServerReceivePacket(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSendConnectResult(Thread *thread, ptr<uint> result, uint kid,
                                   ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSessionRespondSync(Thread *thread, ptr<uint> result, uint kid,
                                    ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSessionGetClientPid(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientInvokeSyncMethod(Thread *thread, ptr<uint> result,
                                        uint kid, ptr<void> params,
                                        uint64_t paramsSz);
SysResult sysIpmiClientConnect(Thread *thread, ptr<uint> result, uint kid,
                               ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSessionGetUserData(Thread *thread, ptr<uint> result, uint kid,
                                    ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiServerGetName(Thread *thread, ptr<uint> result, uint kid,
                               ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientPollEventFlag(Thread *thread, ptr<uint> result,
                                      uint kid, ptr<void> params,
                                      uint64_t paramsSz);
SysResult sysIpmiSessionWaitEventFlag(Thread *thread, ptr<uint> result,
                                      uint kid, ptr<void> params,
                                      uint64_t paramsSz);
SysResult sysIpmiSessionSetEventFlag(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);

} // namespace orbis
