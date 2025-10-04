#include "ipmi.hpp"

#include "AudioOut.hpp"
#include "io-device.hpp"
#include "orbis/KernelContext.hpp"
#include "orbis/osem.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/format.hpp"
#include "rx/hexdump.hpp"
#include "rx/mem.hpp"
#include "rx/print.hpp"
#include "rx/watchdog.hpp"
#include "vfs.hpp"
#include "vm.hpp"
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>
#include <sys/stat.h>

ipmi::IpmiClient ipmi::audioIpmiClient;

template <typename T = std::byte> struct GuestAlloc {
  orbis::ptr<T> guestAddress;

  GuestAlloc(std::size_t size) {
    if (size == 0) {
      guestAddress = nullptr;
    } else {
      guestAddress = orbis::ptr<T>(
          vm::map(nullptr, size, vm::kMapProtCpuRead | vm::kMapProtCpuWrite,
                  vm::kMapFlagPrivate | vm::kMapFlagAnonymous));
    }
  }

  GuestAlloc() : GuestAlloc(sizeof(T)) {}

  GuestAlloc(const T &data) : GuestAlloc() {
    if (orbis::uwrite(guestAddress, data) != orbis::ErrorCode{}) {
      std::abort();
    }
  }

  GuestAlloc(const void *data, std::size_t size) : GuestAlloc(size) {
    if (orbis::uwriteRaw(guestAddress, data, size) != orbis::ErrorCode{}) {
      std::abort();
    }
  }

  GuestAlloc(const GuestAlloc &) = delete;

  GuestAlloc(GuestAlloc &&other) noexcept : guestAddress(other.guestAddress) {
    other.guestAddress = 0;
  }
  GuestAlloc &operator=(GuestAlloc &&other) noexcept {
    std::swap(guestAddress, other.guestAddress);
  }

  ~GuestAlloc() {
    if (guestAddress != 0) {
      vm::unmap(guestAddress, sizeof(T));
    }
  }

  operator orbis::ptr<T>() { return guestAddress; }
  T *operator->() { return guestAddress; }
  operator T &() { return *guestAddress; }
};

orbis::sint ipmi::IpmiClient::sendSyncMessageRaw(
    std::uint32_t method, const std::vector<std::vector<std::byte>> &inData,
    std::vector<std::vector<std::byte>> &outBuf) {
  GuestAlloc<orbis::sint> serverResult;
  GuestAlloc<orbis::IpmiDataInfo> guestInDataArray{sizeof(orbis::IpmiDataInfo) *
                                                   inData.size()};
  GuestAlloc<orbis::IpmiBufferInfo> guestOutBufArray{
      sizeof(orbis::IpmiBufferInfo) * outBuf.size()};

  std::vector<GuestAlloc<std::byte>> guestAllocs;
  guestAllocs.reserve(inData.size() + outBuf.size());

  for (auto &data : inData) {
    auto pointer =
        guestAllocs.emplace_back(data.data(), data.size()).guestAddress;

    guestInDataArray.guestAddress[&data - inData.data()] = {
        .data = pointer, .size = data.size()};
  }

  for (auto &buf : outBuf) {
    auto pointer =
        guestAllocs.emplace_back(buf.data(), buf.size()).guestAddress;

    guestOutBufArray.guestAddress[&buf - outBuf.data()] = {
        .data = pointer, .capacity = buf.size()};
  }

  GuestAlloc params = orbis::IpmiSyncCallParams{
      .method = method,
      .numInData = static_cast<orbis::uint32_t>(inData.size()),
      .numOutData = static_cast<orbis::uint32_t>(outBuf.size()),
      .pInData = guestInDataArray,
      .pOutData = guestOutBufArray,
      .pResult = serverResult,
      .flags = (inData.size() >= 1 || outBuf.size() >= 1) ? 1u : 0u,
  };

  GuestAlloc<orbis::uint> errorCode;
  orbis::sysIpmiClientInvokeSyncMethod(thread, errorCode, kid, params,
                                       sizeof(orbis::IpmiSyncCallParams));

  for (auto &buf : outBuf) {
    auto size = guestOutBufArray.guestAddress[inData.data() - &buf].size;
    buf.resize(size);
  }
  return serverResult;
}

ipmi::IpmiClient ipmi::createIpmiClient(orbis::Thread *thread,
                                        const char *name) {
  orbis::Ref<orbis::IpmiClient> client;
  GuestAlloc config = orbis::IpmiCreateClientConfig{
      .size = sizeof(orbis::IpmiCreateClientConfig),
  };

  orbis::uint kid;

  {
    GuestAlloc<char> guestName{name, std::strlen(name)};
    GuestAlloc params = orbis::IpmiCreateClientParams{
        .name = guestName,
        .config = config,
    };

    GuestAlloc<orbis::uint> result;
    GuestAlloc<orbis::uint> guestKid;
    orbis::sysIpmiCreateClient(thread, guestKid, params,
                               sizeof(orbis::IpmiCreateClientParams));
    kid = guestKid;
  }

  {
    GuestAlloc<orbis::sint> status;
    GuestAlloc params = orbis::IpmiClientConnectParams{.status = status};

    GuestAlloc<orbis::uint> result;
    while (true) {
      auto errc = orbis::sysIpmiClientConnect(
          thread, result, kid, params, sizeof(orbis::IpmiClientConnectParams));
      if (errc.value() == 0) {
        break;
      }

      std::this_thread::sleep_for(std::chrono::microseconds(300));
    }
  }

  return {.clientImpl = std::move(client), .kid = kid, .thread = thread};
}

orbis::Semaphore *ipmi::createSemaphore(std::string_view name, uint32_t attrs,
                                        uint64_t initCount, uint64_t maxCount) {
  auto result =
      orbis::g_context
          .createSemaphore(orbis::kstring(name), attrs, initCount, maxCount)
          .first;
  std::memcpy(result->name, name.data(), name.size());
  result->name[name.size()] = 0;
  return result;
}

orbis::EventFlag *ipmi::createEventFlag(std::string_view name, uint32_t attrs,
                                        uint64_t initPattern) {
  return orbis::g_context
      .createEventFlag(orbis::kstring(name), attrs, initPattern)
      .first;
}

void ipmi::createShm(const char *name, uint32_t flags, uint32_t mode,
                     uint64_t size) {
  orbis::Ref<orbis::File> shm;
  auto shmDevice = orbis::g_context.shmDevice.staticCast<IoDevice>();
  shmDevice->open(&shm, name, flags, mode, nullptr);
  shm->ops->truncate(shm.get(), size, nullptr);
}

orbis::ErrorCode
ipmi::IpmiServer::handle(orbis::IpmiSession *session,
                         orbis::IpmiAsyncMessageHeader *message) {
  std::vector<std::span<std::byte>> inData;
  std::vector<std::vector<std::byte>> outData;
  auto bufLoc = std::bit_cast<std::byte *>(message + 1);

  for (unsigned i = 0; i < message->numInData; ++i) {
    auto size = *std::bit_cast<orbis::uint *>(bufLoc);
    bufLoc += sizeof(orbis::uint);
    inData.push_back({bufLoc, size});
    bufLoc += size;
  }

  orbis::IpmiClient::AsyncResponse response;
  response.methodId = message->methodId + 1;
  response.errorCode = 0;
  orbis::ErrorCode result{};

  if (auto it = asyncMethods.find(message->methodId);
      it != asyncMethods.end()) {
    auto &handler = it->second;

    result = handler(*session, response.errorCode, outData, inData);
  } else {
    rx::println(stderr, "Unimplemented async method {}::{:x}(inBufCount={})",
                session->server->name, unsigned(message->methodId),
                unsigned(message->numInData));

    for (auto in : inData) {
      std::println(stderr, "in {}", in.size());
      rx::hexdump(in);
    }
  }

  for (auto out : outData) {
    response.data.push_back({out.data(), out.data() + out.size()});
  }

  std::lock_guard clientLock(session->client->mutex);
  session->client->asyncResponses.push_front(std::move(response));
  std::fprintf(stderr, "%s:%x: sending async response\n",
               session->client->name.c_str(), message->methodId);
  session->client->asyncResponseCv.notify_all(session->client->mutex);
  return result;
}
orbis::ErrorCode
ipmi::IpmiServer::handle(orbis::IpmiSession *session,
                         orbis::IpmiServer::Packet &packet,
                         orbis::IpmiSyncMessageHeader *message) {
  auto bufLoc = std::bit_cast<std::byte *>(message + 1);
  std::vector<std::span<std::byte>> inData;
  std::vector<std::vector<std::byte>> outData;
  for (unsigned i = 0; i < message->numInData; ++i) {
    auto size = *std::bit_cast<orbis::uint *>(bufLoc);
    bufLoc += sizeof(orbis::uint);
    inData.emplace_back(bufLoc, size);
    bufLoc += size;
  }

  for (unsigned i = 0; i < message->numOutData; ++i) {
    auto size = *std::bit_cast<orbis::uint *>(bufLoc);
    bufLoc += sizeof(orbis::uint);
    outData.emplace_back(size);
  }

  orbis::IpmiSession::SyncResponse response;
  response.errorCode = 0;
  orbis::ErrorCode result{};

  if (auto it = syncMethods.find(message->methodId); it != syncMethods.end()) {
    auto &handler = it->second;

    result = handler(*session, response.errorCode, outData, inData);
  } else {
    std::println(
        stderr,
        "Unimplemented sync method {}::{:x}(inBufCount={}, outBufCount={})",
        session->server->name, unsigned(message->methodId),
        unsigned(message->numInData), unsigned(message->numOutData));

    for (auto in : inData) {
      std::println(stderr, "in {}", in.size());
      rx::hexdump(in);
    }

    for (auto &out : outData) {
      std::println(stderr, "out {:x}", out.size());
    }

    for (auto out : outData) {
      std::memset(out.data(), 0, out.size());
    }
    // TODO:
    // response.errorCode = message->numOutData == 0 ||
    //                      (message->numOutData == 1 && outData[0].empty())
    //                  ? 0
    //                  : -1,
  }

  response.callerTid = packet.clientTid;
  for (auto out : outData) {
    response.data.push_back({out.data(), out.data() + out.size()});
  }

  std::lock_guard lock(session->mutex);
  session->syncResponses.push_front(std::move(response));
  session->responseCv.notify_all(session->mutex);

  return result;
}

ipmi::IpmiServer &ipmi::createIpmiServer(orbis::Process *process,
                                         const char *name) {
  orbis::IpmiCreateServerConfig config{};
  orbis::Ref<orbis::IpmiServer> serverImpl;
  orbis::ipmiCreateServer(process, nullptr, name, config, serverImpl);
  auto server = std::make_shared<ipmi::IpmiServer>();
  server->serverImpl = serverImpl;

  std::thread{[server, serverImpl, name] {
    pthread_setname_np(pthread_self(), name);
    while (true) {
      orbis::IpmiServer::Packet packet;
      {
        std::lock_guard lock(serverImpl->mutex);

        while (serverImpl->packets.empty()) {
          serverImpl->receiveCv.wait(serverImpl->mutex);
        }

        packet = std::move(serverImpl->packets.front());
        serverImpl->packets.pop_front();
      }

      if (packet.info.type == 1) {
        std::lock_guard serverLock(serverImpl->mutex);

        for (auto it = serverImpl->connectionRequests.begin();
             it != serverImpl->connectionRequests.end(); ++it) {
          auto &conReq = *it;
          std::lock_guard clientLock(conReq.client->mutex);
          if (conReq.client->session != nullptr) {
            continue;
          }

          auto session = orbis::knew<orbis::IpmiSession>();
          if (session == nullptr) {
            break;
          }

          session->client = conReq.client;
          session->server = serverImpl;
          conReq.client->session = session;

          for (auto &message : server->messages) {
            conReq.client->messageQueues[0].messages.push_back(
                orbis::kvector<std::byte>(message.data(),
                                          message.data() + message.size()));
          }

          conReq.client->connectionStatus = 0;
          conReq.client->sessionCv.notify_all(conReq.client->mutex);
          conReq.client->connectCv.notify_all(conReq.client->mutex);
          break;
        }

        continue;
      }

      if ((packet.info.type & ~0x8010) == 0x41) {
        auto msgHeader = std::bit_cast<orbis::IpmiSyncMessageHeader *>(
            packet.message.data());
        auto process = orbis::g_context.findProcessById(msgHeader->pid);
        if (process == nullptr) {
          continue;
        }
        auto client = orbis::g_context.ipmiMap.get(packet.info.clientKid)
                          .cast<orbis::IpmiClient>();
        if (client == nullptr) {
          continue;
        }
        auto session = client->session;
        if (session == nullptr) {
          continue;
        }

        server->handle(client->session.get(), packet, msgHeader);
        packet = {};
        continue;
      }

      if ((packet.info.type & ~0x10) == 0x43) {
        auto msgHeader = (orbis::IpmiAsyncMessageHeader *)packet.message.data();
        auto process = orbis::g_context.findProcessById(msgHeader->pid);
        if (process == nullptr) {
          continue;
        }
        auto client = orbis::g_context.ipmiMap.get(packet.info.clientKid)
                          .cast<orbis::IpmiClient>();
        if (client == nullptr) {
          continue;
        }
        auto session = client->session;
        if (session == nullptr) {
          continue;
        }

        server->handle(client->session.get(), msgHeader);
        continue;
      }

      std::println(stderr, "IPMI: Unhandled packet {}::{}", serverImpl->name,
                   packet.info.type);
    }
  }}.detach();

  return *server;
}

void ipmi::createMiniSysCoreObjects(orbis::Process *) {
  createEventFlag("SceBootStatusFlags", 0x121, ~0ull);
}

void ipmi::createSysAvControlObjects(orbis::Process *process) {
  createIpmiServer(process, "SceAvSettingIpc");

  createIpmiServer(process, "SceAvCaptureIpc");
  createEventFlag("SceAvCaptureIpc", 0x121, 0);
  createEventFlag("SceAvSettingEvf", 0x121, 0xffff00000000);

  createShm("/SceAvSetting", 0xa02, 0x1a4, 4096);
}

struct SceSysAudioSystemThreadArgs {
  uint32_t threadId;
};

struct SceSysAudioSystemPortAndThreadArgs {
  uint32_t audioPort;
  uint32_t threadId;
};

void ipmi::createAudioSystemObjects(orbis::Process *process) {
  auto audioOut = orbis::Ref<AudioOut>(orbis::knew<AudioOut>());

  createIpmiServer(process, "SceSysAudioSystemIpc")
      .addSyncMethod<SceSysAudioSystemThreadArgs>(
          0x12340000,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemCreateControl",
                           args.threadId);
            audioOut->channelInfo.idControl = args.threadId;
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemThreadArgs>(
          0x1234000f,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemOpenMixFlag", args.threadId);
            // very bad
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "sceAudioOutMix%x",
                          args.threadId);
            auto [eventFlag, inserted] =
                orbis::g_context.createEventFlag(buffer, 0x100, 0);

            if (!inserted) {
              return 17; // FIXME: verify
            }

            audioOut->channelInfo.evf = eventFlag;
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemPortAndThreadArgs>(
          0x12340001,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemOpenPort", args.threadId,
                           args.audioPort);
            audioOut->channelInfo.port = args.audioPort;
            audioOut->channelInfo.channel = args.threadId;
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemPortAndThreadArgs>(
          0x12340002,
          [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemStartListening",
                           args.threadId, args.audioPort);

            audioOut->start();
            return 0;
          })
      .addSyncMethod<SceSysAudioSystemPortAndThreadArgs>(
          0x12340006, [=](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceSysAudioSystemStopListening",
                           args.audioPort, args.threadId);
            // TODO: implement
            return 0;
          });
}

struct SceMbusIpcAddHandleByUserIdMethodArgs {
  orbis::uint32_t deviceType; // 0 - pad, 1 - aout, 2 - ain, 4 - camera, 6 - kb,
                              // 7 - mouse, 8 - vr
  orbis::uint32_t deviceId;
  orbis::uint32_t userId;
  orbis::uint32_t type;
  orbis::uint32_t index;
  orbis::uint32_t reserved;
  orbis::uint32_t pid;
};

static_assert(sizeof(SceMbusIpcAddHandleByUserIdMethodArgs) == 0x1c);

struct SceUserServiceEvent {
  std::uint32_t eventType; // 0 - login, 1 - logout
  std::uint32_t user;
};

void ipmi::createSysCoreObjects(orbis::Process *process) {
  createIpmiServer(process, "SceMbusIpc")
      .addSyncMethod<SceMbusIpcAddHandleByUserIdMethodArgs>(
          0xce110007, [](const auto &args) -> std::int32_t {
            ORBIS_LOG_TODO("IPMI: SceMbusIpcAddHandleByUserId", args.deviceType,
                           args.deviceId, args.userId, args.type, args.index,
                           args.reserved, args.pid);
            if (args.deviceType == 1) {
              struct HandleA {
                int32_t pid;
                int32_t port;
                int32_t unk0 = 0x20100000;
                int32_t unk1 = 1;
              } handleA;
              handleA.pid = args.pid;
              handleA.port = args.deviceId;
              audioIpmiClient.sendSyncMessage(0x1234000a, handleA);
              struct HandleC {
                int32_t pid;
                int32_t port;
                int32_t unk0 = 1;
                int32_t unk1 = 0;
                int32_t unk2 = 1;
                int32_t unk3 = 0;
                int32_t unk4 = 0;
                int32_t unk5 = 0;
                int32_t unk6 = 0;
                int32_t unk7 = 1;
                int32_t unk8 = 0;
              } handleC;
              handleC.pid = args.pid;
              handleC.port = args.deviceId;
              audioIpmiClient.sendSyncMessage(0x1234000c, handleC);
            }
            return 0;
          });
  createIpmiServer(process, "SceSysCoreApp");
  createIpmiServer(process, "SceSysCoreApp2");
  createIpmiServer(process, "SceMDBG0SRV");

  createSemaphore("SceSysCoreProcSpawnSema", 0x101, 0, 1);
  createSemaphore("SceTraceMemorySem", 0x100, 1, 1);
  createSemaphore("SceSysCoreEventSemaphore", 0x101, 0, 0x2d2);
  createSemaphore("SceSysCoreProcSema", 0x101, 0, 1);
  createSemaphore("AppmgrCoredumpHandlingEventSema", 0x101, 0, 4);

  createEventFlag("SceMdbgVrTriggerDump", 0x121, 0);
}

void ipmi::createGnmCompositorObjects(orbis::Process *) {
  createEventFlag("SceCompositorCrashEventFlags", 0x122, 0);
  createEventFlag("SceCompositorEventflag", 0x122, 0);
  createEventFlag("SceCompositorResetStatusEVF", 0x122, 0);

  createShm("/tmp/SceHmd/Vr2d_shm_pass", 0xa02, 0x1b6, 16384);
}

void ipmi::createShellCoreObjects(orbis::Process *process) {
  auto fmtHex = [](auto value, bool upperCase = false) {
    if (upperCase) {
      return rx::format("{:08X}", value);
    }
    return rx::format("{:08x}", value);
  };

  createIpmiServer(process, "SceSystemLoggerService");
  createIpmiServer(process, "SceLoginMgrServer");
  int lnsStatusServer;

  if (orbis::g_context.fwType == orbis::FwType::Ps5) {
    lnsStatusServer = 0x30010;
  } else {
    if (orbis::g_context.fwSdkVersion > 0x6000000) {
      lnsStatusServer = 0x30013;
    } else {
      lnsStatusServer = 0x30010;
    }
  }
  createIpmiServer(process, "SceLncService")
      .addSyncMethod(lnsStatusServer,
                     [](void *out, std::uint64_t &size) -> std::int32_t {
                       struct SceLncServiceAppStatus {
                         std::uint32_t unk0;
                         std::uint32_t unk1;
                         std::uint32_t unk2;
                       };

                       if (size < sizeof(SceLncServiceAppStatus)) {
                         return -1;
                       }

                       *(SceLncServiceAppStatus *)out = {
                           .unk0 = 1u,
                           .unk1 = 0u,
                           .unk2 = 5u,
                       };

                       size = sizeof(SceLncServiceAppStatus);
                       return 0;
                     })
      .addSyncMethodStub(
          orbis::g_context.fwSdkVersion > 0x6000000 ? 0x30033 : 0x3002e,
          []() -> std::int32_t {
            auto commonDialog = std::get<0>(orbis::g_context.dialogs.front());
            auto currentDialogId =
                *reinterpret_cast<std::int16_t *>(commonDialog + 4);
            auto currentDialog = std::get<0>(orbis::g_context.dialogs.back());
            if (currentDialogId == 5) {
              std::int32_t titleSize = 8192;
              std::int32_t buttonNameSize = 64;
              std::string dialogTitle(
                  reinterpret_cast<char *>(currentDialog + 0x4e4), titleSize);
              std::string buttonOk(
                  reinterpret_cast<char *>(currentDialog + 0x2510),
                  buttonNameSize);
              std::string buttonCancel(
                  reinterpret_cast<char *>(currentDialog + 0x2550),
                  buttonNameSize);
              auto buttonType =
                  *reinterpret_cast<std::uint8_t *>(currentDialog + 0x488);
              ORBIS_LOG_TODO("Activate message dialog", dialogTitle.data(),
                             buttonOk.data(), buttonCancel.data(),
                             (std::int16_t)buttonType);
              // ignore dialogs without buttons
              if (buttonType != 2 && buttonType != 5 && buttonType != 6) {
                *reinterpret_cast<std::uint8_t *>(currentDialog + 0x18) =
                    1; // finished state
                *reinterpret_cast<std::int32_t *>(currentDialog + 0x30) =
                    0; // result code
                *reinterpret_cast<std::uint8_t *>(currentDialog + 0x24ec) =
                    1; // pressed button type
              }
            } else {
              ORBIS_LOG_TODO("Activate unsupported dialog", currentDialogId);
            }
            return 0;
          })
      .addSyncMethod(
          orbis::g_context.fwSdkVersion > 0x6000000 ? 0x30044 : 0x3003f,
          [=](std::vector<std::vector<std::byte>>,
              const std::vector<std::span<std::byte>> &inData) -> std::int32_t {
            struct InitDialogArgs {
              char *name;
              size_t len;
            };
            auto args = (InitDialogArgs *)inData.data();
            // maybe it's not necessary, but they add 1 to len
            std::string realName(args->name, args->len - 1);
            auto hostPath = rx::getShmGuestPath(realName);
            ORBIS_LOG_TODO("Register dialog", inData.data(), inData.size(),
                           realName.data());
            int shmFd =
                ::open(hostPath.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
            if (shmFd == -1) {
              perror("shm_open");
              std::abort();
            }

            struct stat controlStat;
            if (::fstat(shmFd, &controlStat)) {
              perror("fstat");
              std::abort();
            }

            auto shmAddress = reinterpret_cast<std::uint8_t *>(
                rx::mem::map(nullptr, controlStat.st_size,
                             PROT_READ | PROT_WRITE, MAP_SHARED, shmFd));
            if (shmAddress == MAP_FAILED) {
              perror("mmap");
              std::abort();
            }
            orbis::g_context.dialogs.emplace_back(shmAddress,
                                                  controlStat.st_size);
            return 0;
          })
      .addSyncMethod(
          orbis::g_context.fwSdkVersion > 0x6000000 ? 0x30045 : 0x30040,
          [=](std::vector<std::vector<std::byte>>,
              const std::vector<std::span<std::byte>> &inData) -> std::int32_t {
            if (!orbis::g_context.dialogs.empty()) {
              auto currentDialogAddr =
                  std::get<0>(orbis::g_context.dialogs.back());
              auto currentDialogSize =
                  std::get<1>(orbis::g_context.dialogs.back());
              ORBIS_LOG_TODO("Unmap shm after unlinking", currentDialogAddr,
                             currentDialogSize);
              rx::mem::unmap(currentDialogAddr, currentDialogSize);
              orbis::g_context.dialogs.pop_back();
            }
            return 0;
          });
  createIpmiServer(process, "SceAppMessaging");
  createIpmiServer(process, "SceShellCoreUtil");
  createIpmiServer(process, "SceNetCtl");
  createIpmiServer(process, "SceNpMgrIpc")
      .addSyncMethod(
          0,
          [=](void *out, std::uint64_t &size) -> std::int32_t {
            std::string_view result = "SceNpMgrEvf";
            if (size < result.size() + 1) {
              return 0x8002'0000 + static_cast<int>(orbis::ErrorCode::INVAL);
            }
            std::strncpy((char *)out, result.data(), result.size() + 1);
            size = result.size() + 1;
            orbis::g_context.createEventFlag(orbis::kstring(result), 0x200, 0);
            return 0;
          })
      .addSyncMethodStub(0xd);
  createIpmiServer(process, "SceNpService")
      .addSyncMethod<std::uint32_t>(
          0, [=](void *, std::uint64_t &, std::uint32_t) { return 0; })
      .addSyncMethod(0xa0001,
                     [=](void *out, std::uint64_t &size) -> std::int32_t {
                       if (size < 1) {
                         return 0x8002'0000 +
                                static_cast<int>(orbis::ErrorCode::INVAL);
                       }
                       size = 1;
                       *reinterpret_cast<std::uint8_t *>(out) = 1;
                       return 0;
                     })
      .addSyncMethod(0xa0002,
                     [=](void *out, std::uint64_t &size) -> std::int32_t {
                       if (size < 1) {
                         return 0x8002'0000 +
                                static_cast<int>(orbis::ErrorCode::INVAL);
                       }
                       size = 1;
                       *reinterpret_cast<std::uint8_t *>(out) = 1;
                       return 0;
                     })
      .addSyncMethod<std::uint32_t, std::uint32_t>(
          0xd0000, // sceNpTpipIpcClientGetShmIndex
          [=](std::uint32_t &shmIndex, std::uint32_t) -> std::int32_t {
            shmIndex = 0;
            return 0;
          });

  createIpmiServer(process, "SceNpTrophyIpc")
      .addSyncMethod(2,
                     [](std::vector<std::vector<std::byte>> &out,
                        const std::vector<std::span<std::byte>> &) {
                       if (out.size() != 1 ||
                           out[0].size() < sizeof(std::uint32_t)) {
                         return orbis::ErrorCode::INVAL;
                       }
                       out = {toBytes<std::uint32_t>(0)};
                       return orbis::ErrorCode{};
                     })
      .addAsyncMethod(0x30040,
                      [](orbis::IpmiSession &session,
                         std::vector<std::vector<std::byte>> &,
                         const std::vector<std::span<std::byte>> &) {
                        session.client->eventFlags[0].set(1);
                        return orbis::ErrorCode{};
                      })
      .addSyncMethod(0x90000,
                     [](std::vector<std::vector<std::byte>> &out,
                        const std::vector<std::span<std::byte>> &) {
                       if (out.size() != 1 ||
                           out[0].size() < sizeof(std::uint32_t)) {
                         return orbis::ErrorCode::INVAL;
                       }
                       out = {toBytes<std::uint32_t>(1)};
                       return orbis::ErrorCode{};
                     })
      .addSyncMethod(0x90003,
                     [](std::vector<std::vector<std::byte>> &out,
                        const std::vector<std::span<std::byte>> &) {
                       if (out.size() != 1 ||
                           out[0].size() < sizeof(std::uint32_t)) {
                         return orbis::ErrorCode::INVAL;
                       }
                       out = {toBytes<std::uint32_t>(1)};
                       return orbis::ErrorCode{};
                     })
      .addAsyncMethod(0x90024,
                      [](orbis::IpmiSession &,
                         std::vector<std::vector<std::byte>> &out,
                         const std::vector<std::span<std::byte>> &) {
                        out.push_back(toBytes<std::uint32_t>(0));
                        // session.client->eventFlags[0].set(1);
                        return orbis::ErrorCode{};
                      })
      .addAsyncMethod(0x90026, [](orbis::IpmiSession &session,
                                  std::vector<std::vector<std::byte>> &,
                                  const std::vector<std::span<std::byte>> &) {
        session.client->eventFlags[0].set(1);
        return orbis::ErrorCode{};
      });
  createIpmiServer(process, "SceNpUdsIpc");
  createIpmiServer(process, "SceLibNpRifMgrIpc");
  createIpmiServer(process, "SceNpPartner001");
  createIpmiServer(process, "SceNpPartnerSubs");
  createIpmiServer(process, "SceNpGameIntent");
  createIpmiServer(process, "SceBgft");
  createIpmiServer(process, "SceCntMgrService");
  createIpmiServer(process, "ScePlayGo");
  createIpmiServer(process, "SceCompAppProxyUtil");
  createIpmiServer(process, "SceShareSpIpcService");
  createIpmiServer(process, "SceRnpsAppMgr");
  createIpmiServer(process, "SceUpdateService");
  createIpmiServer(process, "ScePatchChecker");
  createIpmiServer(process, "SceMorpheusUpdService");
  createIpmiServer(process, "ScePsmSharedDmem");

  auto saveDataSem =
      createSemaphore("SceSaveData0000000000000001", 0x101, 0, 1);
  auto saveDataSem_0 =
      createSemaphore("SceSaveData0000000000000001_0", 0x101, 0, 1);
  createShm("SceSaveData0000000000000001_0", 0x202, 0x1b6, 0x40000);
  createShm("SceSaveDataI0000000000000001", 0x202, 0x1b6, 43008);
  createShm("SceSaveDataI0000000000000001_0", 0x202, 0x1b6, 43008);
  createShm("SceNpPlusLogger", 0x202, 0x1b6, 0x40000);
  auto ruiEvf = createEventFlag("SceSaveDataMemoryRUI00000010", 0x120,
                                0x1000000100010000);

  createIpmiServer(process, "SceSaveData")
      .addSyncMethod(0x12340000,
                     [=](void *, std::uint64_t &) -> std::int32_t {
                       ruiEvf->set(~0ull);
                       {
                         saveDataSem->value++;
                         saveDataSem->cond.notify_one(saveDataSem->mtx);
                       }
                       {
                         saveDataSem_0->value++;
                         saveDataSem_0->cond.notify_one(saveDataSem_0->mtx);
                       }
                       return 0;
                     })
      .addSyncMethod(
          0x12340001,
          [](std::vector<std::vector<std::byte>> &outData,
             const std::vector<std::span<std::byte>> &inData) -> std::int32_t {
            std::println(stderr, "SceSaveData: 0x12340001");
            if (inData.size() != 2 || outData.size() != 2) {
              return 0x8002000 +
                     static_cast<std::uint32_t>(orbis::ErrorCode::INVAL);
            }
            if (inData[0].size() != sizeof(orbis::uint64_t) ||
                outData[1].size() != sizeof(orbis::uint64_t)) {
              return 0x8002000 +
                     static_cast<std::uint32_t>(orbis::ErrorCode::INVAL);
            }

            auto outputLen =
                *reinterpret_cast<orbis::uint64_t *>(inData[0].data());

            if (outputLen != outData[0].size()) {
              return 0x8002000 +
                     static_cast<std::uint32_t>(orbis::ErrorCode::INVAL);
            }

            struct Request {
              orbis::uint32_t unk0;
              orbis::uint32_t id;
              orbis::uint32_t unk1[31];
            };

            static_assert(sizeof(Request) == 132);

            if (inData[1].size() != sizeof(Request)) {
              return 0x8002000 +
                     static_cast<std::uint32_t>(orbis::ErrorCode::INVAL);
            }

            auto request = reinterpret_cast<Request *>(inData[1].data());

            std::println(stderr, "SceSaveData: 0x12340001, message {}",
                         request->id);

            for (std::size_t index = 0; auto &in : inData) {
              std::println(stderr, "in {} - {}", index++, in.size());
              rx::hexdump(in);
            }

            for (std::size_t index = 0; auto &out : outData) {
              std::println(stderr, "out {} - {}", index++, out.size());
            }

            if (request->id == 2) {
              return 0;
            }

            if (request->id == 3) {
              struct MountInfo {
                std::uint64_t blocks;
                std::uint64_t freeBlocks;
              };

              std::memset(outData[0].data(), 0xff, outData[0].size());

              auto info = (MountInfo *)outData[0].data();
              info->blocks = 1024 * 32;
              info->freeBlocks = 1024 * 16;
              return 0;
            }

            if (request->id == 4) {
              return 0;
            }

            if (request->id == 6) {
              struct Entry {
                char string[32];
              };

              struct SearchResults {
                std::uint32_t totalCount;
                std::uint32_t count;
                Entry entries[];
              };

              std::uint32_t fillOffset = 4 + sizeof(Entry) + 1024;

              std::memset(outData[0].data() + fillOffset, 0xff,
                          outData[0].size() - fillOffset);

              auto results = (SearchResults *)outData[0].data();
              std::vector<std::string> searchResults;
              searchResults.emplace_back("TEST");
              results->totalCount = searchResults.size();

              Entry *entries = results->entries;
              results->count = searchResults.size();

              for (auto &str : searchResults) {
                std::strncpy(entries->string, str.data(),
                             sizeof(entries->string));
                entries->string[std::size(entries->string) - 1] = 0;

                entries++;
              }

              return 0;
            }

            if (request->id == 7) {
              return 0;
            }

            if (request->id == 8) {
              return 0;
            }

            if (request->id == 9) {
              return 0;
            }

            if (request->id == 10) {
              return 0;
            }

            if (request->id == 1 || request->id == 60) {
              {
                auto [dev, devPath] = vfs::get("/app0");
                if (auto hostFs = dev.cast<HostFsDevice>()) {
                  std::error_code ec;
                  auto saveDir = hostFs->hostPath + "/.rpcsx/savedata/";
                  if (!std::filesystem::exists(saveDir)) {
                    return 0x8002'0000 +
                           static_cast<int>(orbis::ErrorCode::NOENT);
                  }
                }
              }

              // umount
              std::string_view result = "/savedata";
              if (outData[0].size() < result.size() + 1) {
                return 0x8002'0000 + static_cast<int>(orbis::ErrorCode::INVAL);
              }
              std::strncpy((char *)outData[0].data(), result.data(),
                           result.size() + 1);
              outData[0].resize(result.size() + 1);
              orbis::g_context.createEventFlag(orbis::kstring(result), 0x200,
                                               0);

              outData[1] = toBytes<orbis::uint64_t>(0);
              return 0;
            }

            return 0x8002000 +
                   static_cast<std::uint32_t>(orbis::ErrorCode::INVAL);
          })
      .addSyncMethod(0x12340002, [](void *, std::uint64_t &) -> std::int32_t {
        {
          auto [dev, devPath] = vfs::get("/app0");
          if (auto hostFs = dev.cast<HostFsDevice>()) {
            std::error_code ec;
            auto saveDir = hostFs->hostPath + "/.rpcsx/savedata/";
            std::filesystem::create_directories(saveDir, ec);
            vfs::mount("/savedata/", createHostIoDevice(saveDir, "/savedata/"));
          }
        }
        return 0;
      });

  createIpmiServer(process, "SceStickerCoreServer");
  createIpmiServer(process, "SceDbRecoveryShellCore");
  createIpmiServer(process, "SceUserService")
      .sendMsg(SceUserServiceEvent{.eventType = 0, .user = 1})
      .addSyncMethod(0x30011,
                     [](void *ptr, std::uint64_t &size) -> std::int32_t {
                       if (size < sizeof(orbis::uint32_t)) {
                         return 0x8000'0000;
                       }

                       *(orbis::uint32_t *)ptr = 1;
                       size = sizeof(orbis::uint32_t);
                       return 0;
                     });

  createIpmiServer(process, "SceDbPreparationServer");
  createIpmiServer(process, "SceScreenShot");
  createIpmiServer(process, "SceAppDbIpc");
  createIpmiServer(process, "SceAppInst");
  createIpmiServer(process, "SceAppContent")
      .addSyncMethod<orbis::uint32_t, orbis::uint32_t>(
          0x20001,
          [](orbis::uint32_t &out, orbis::uint32_t param) -> std::int32_t {
            switch (param) {
            case 0: // sku
              out = 3;
              return 0;

            case 1: // user defined param 0
            case 2: // user defined param 1
            case 3: // user defined param 2
            case 4: // user defined param 3
              ORBIS_LOG_ERROR("SceAppContent: get user defined param");
              out = 0;
              return 0;
            }

            return 0x8002000 +
                   static_cast<std::uint32_t>(orbis::ErrorCode::INVAL);
          });

  createIpmiServer(process, "SceNpEntAccess");
  createIpmiServer(process, "SceMwIPMIServer");
  createIpmiServer(process, "SceAutoMounterIpc");
  createIpmiServer(process, "SceBackupRestoreUtil");
  createIpmiServer(process, "SceDataTransfer");
  createIpmiServer(process, "SceEventService");
  createIpmiServer(process, "SceShareFactoryUtil");
  createIpmiServer(process, "SceCloudConnectManager");
  createIpmiServer(process, "SceHubAppUtil");
  createIpmiServer(process, "SceTcIPMIServer");

  createSemaphore("SceLncSuspendBlock00000001", 0x101, 1, 1);
  createSemaphore("SceAppMessaging00000001", 0x100, 1, 0x7fffffff);

  createEventFlag("SceAutoMountUsbMass", 0x120, 0);
  createEventFlag("SceLoginMgrUtilityEventFlag", 0x112, 0);
  createEventFlag("SceLoginMgrSharePlayEventFlag", 0x112, 0);
  createEventFlag("SceLoginMgrServerHmdConnect", 0x112, 0);
  createEventFlag("SceLoginMgrServerDialogRequest", 0x112, 0);
  createEventFlag("SceLoginMgrServerDialogResponse", 0x112, 0);
  createEventFlag("SceGameLiveStreamingSpectator", 0x120, 0x8000000000000000);
  createEventFlag("SceGameLiveStreamingUserId", 0x120, 0x8000000000000000);
  createEventFlag("SceGameLiveStreamingMsgCount", 0x120, 0x8000000000000000);
  createEventFlag("SceGameLiveStreamingBCCtrl", 0x120, 0);
  createEventFlag("SceGameLiveStreamingEvntArg", 0x120, 0);
  createEventFlag("SceLncUtilSystemStatus", 0x120, 0);
  createEventFlag("SceShellCoreUtilRunLevel", 0x100, 0);
  createEventFlag("SceSystemStateMgrInfo", 0x120, 0x10000000a);
  createEventFlag("SceSystemStateMgrStatus", 0x120, 0);
  createEventFlag("SceAppInstallerEventFlag", 0x120, 0);
  createEventFlag("SceShellCoreUtilPowerControl", 0x120, 0x400000);
  createEventFlag("SceShellCoreUtilAppFocus", 0x120, 1);
  createEventFlag("SceShellCoreUtilCtrlFocus", 0x120, 0);
  createEventFlag("SceShellCoreUtilUIStatus", 0x120, 0x20001);
  createEventFlag("SceShellCoreUtilDevIdxBehavior", 0x120, 0);
  createEventFlag("SceNpMgrVshReq", 0x121, 0);
  createEventFlag("SceNpIdMapperVshReq", 0x121, 0);
  createEventFlag("SceRtcUtilTzdataUpdateFlag", 0x120, 0);
  createEventFlag("SceDataTransfer", 0x120, 0);

  createEventFlag("SceLncUtilAppStatus00000000", 0x100, 0);
  createEventFlag("SceLncUtilAppStatus1", 0x100, 0);
  createEventFlag("SceAppMessaging1", 0x120, 1);
  createEventFlag("SceShellCoreUtil1", 0x120, 0x3f8c);
  createEventFlag("SceNpScoreIpc_" + fmtHex(process->pid), 0x120, 0);
  createEventFlag("/vmicDdEvfAin", 0x120, 0);

  createSemaphore("SceAppMessaging1", 0x101, 1, 0x7fffffff);
  createSemaphore("SceLncSuspendBlock1", 0x101, 1, 10000);

  createShm("SceGlsSharedMemory", 0x202, 0x1a4, 262144);
  createShm("SceShellCoreUtil", 0x202, 0x1a4, 16384);
  createShm("SceNpTpip", 0x202, 0x1ff, 43008);

  createShm("vmicDdShmAin", 0x202, 0x1b6, 43008);

  createSemaphore("SceNpTpip 0", 0x101, 0, 1);
}
