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
    ulong inputSize;
    uint type;
    uint clientKid;
    ptr<void> eventHandler;
  };

  static_assert(sizeof(IpmiPacketInfo) == 0x18);

  struct Packet {
    IpmiPacketInfo info;
    lwpid_t clientTid;
    Ref<IpmiSession> session;
    kvector<std::byte> message;
  };

  struct ConnectionRequest {
    Ref<IpmiClient> client;
    slong clientTid{};
    slong clientPid{};
    slong serverTid{};
  };

  kmap<std::uint32_t, std::uint32_t> tidToClientTid;
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
  struct MessageQueue {
    shared_cv messageCv;
    kdeque<kvector<std::byte>> messages;
  };

  struct AsyncResponse {
    uint methodId;
    sint errorCode;
    kvector<kvector<std::byte>> data;
  };

  kstring name;
  ptr<void> clientImpl;
  ptr<void> userData;
  Ref<IpmiSession> session;
  shared_mutex mutex;
  shared_cv sessionCv;
  Process *process;
  kdeque<MessageQueue> messageQueues;
  kdeque<EventFlag> eventFlags;
  kdeque<AsyncResponse> asyncResponses;

  explicit IpmiClient(kstring name) : name(std::move(name)) {}
};

struct IpmiSession : RcBase {
  struct SyncResponse {
    sint errorCode;
    std::uint32_t callerTid;
    kvector<kvector<std::byte>> data;
  };

  ptr<void> sessionImpl;
  ptr<void> userData;
  Ref<IpmiClient> client;
  Ref<IpmiServer> server;
  shared_mutex mutex;
  shared_cv responseCv;
  kdeque<SyncResponse> syncResponses;
  shared_cv connectCv;
  uint expectedOutput{0};
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

struct IpmiCreateClientConfig {
  orbis::uint64_t size;
  orbis::uint32_t unk[80];
  orbis::ptr<void> userData;
};

static_assert(sizeof(IpmiCreateClientConfig) == 0x150);

struct IpmiBufferInfo {
  ptr<void> data;
  uint64_t capacity;
  uint64_t size;
};

struct IpmiDataInfo {
  ptr<void> data;
  uint64_t size;
};

// static_assert(sizeof(IpmiBufferInfo) == 0x18);
// static_assert(sizeof(IpmiDataInfo) == 0x10);

struct [[gnu::packed]] IpmiSyncMessageHeader {
  orbis::ptr<void> sessionImpl;
  orbis::uint pid;
  orbis::uint methodId;
  orbis::uint numInData;
  orbis::uint numOutData;
};

struct [[gnu::packed]] IpmiAsyncMessageHeader {
  orbis::ptr<void> sessionImpl;
  orbis::uint methodId;
  orbis::uint pid;
  orbis::uint numInData;
};

static_assert(sizeof(IpmiSyncMessageHeader) == 0x18);

ErrorCode ipmiCreateClient(Process *proc, void *clientImpl, const char *name,
                           const IpmiCreateClientConfig &config,
                           Ref<IpmiClient> &result);
ErrorCode ipmiCreateServer(Process *proc, void *serverImpl, const char *name,
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
SysResult sysIpmiClientInvokeAsyncMethod(Thread *thread, ptr<uint> result,
                                         uint kid, ptr<void> params,
                                         uint64_t paramsSz);
SysResult sysImpiSessionRespondAsync(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientTryGetResult(Thread *thread, ptr<uint> result, uint kid,
                                    ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientGetMessage(Thread *thread, ptr<uint> result, uint kid,
                                  ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientTryGetMessage(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSessionTrySendMessage(Thread *thread, ptr<uint> result,
                                       uint kid, ptr<void> params,
                                       uint64_t paramsSz);
SysResult sysIpmiClientDisconnect(Thread *thread, ptr<uint> result, uint kid,
                                  ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSessionGetClientPid(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientInvokeSyncMethod(Thread *thread, ptr<uint> result,
                                        uint kid, ptr<void> params,
                                        uint64_t paramsSz);
SysResult sysIpmiClientConnect(Thread *thread, ptr<uint> result, uint kid,
                               ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSessionGetClientAppId(Thread *thread, ptr<uint> result,
                                       uint kid, ptr<void> params,
                                       uint64_t paramsSz);
SysResult sysIpmiSessionGetUserData(Thread *thread, ptr<uint> result, uint kid,
                                    ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiServerGetName(Thread *thread, ptr<uint> result, uint kid,
                               ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientGetName(Thread *thread, ptr<uint> result, uint kid,
                               ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientWaitEventFlag(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiClientPollEventFlag(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);
SysResult sysIpmiSessionSetEventFlag(Thread *thread, ptr<uint> result, uint kid,
                                     ptr<void> params, uint64_t paramsSz);

} // namespace orbis
