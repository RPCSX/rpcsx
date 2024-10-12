#pragma once

#include "orbis/thread/Process.hpp"

namespace ipmi {
template <typename T> std::vector<std::byte> toBytes(const T &value) {
  std::vector<std::byte> result(sizeof(T));
  std::memcpy(result.data(), &value, sizeof(value));
  return result;
}

struct IpmiClient {
  orbis::Ref<orbis::IpmiClient> clientImpl;
  orbis::uint kid;
  orbis::Thread *thread;

  orbis::sint
  sendSyncMessageRaw(std::uint32_t method,
                     const std::vector<std::vector<std::byte>> &inData,
                     std::vector<std::vector<std::byte>> &outBuf);

  template <typename... InputTypes>
  orbis::sint sendSyncMessage(std::uint32_t method,
                              const InputTypes &...input) {
    std::vector<std::vector<std::byte>> outBuf;
    return sendSyncMessageRaw(method, {toBytes(input)...}, outBuf);
  }

  template <typename... OutputTypes, typename... InputTypes>
    requires((sizeof...(OutputTypes) > 0) || sizeof...(InputTypes) == 0)
  std::tuple<OutputTypes...> sendSyncMessage(std::uint32_t method,
                                             InputTypes... input) {
    std::vector<std::vector<std::byte>> outBuf{sizeof(OutputTypes)...};
    sendSyncMessageRaw(method, {toBytes(input)...}, outBuf);
    std::tuple<OutputTypes...> output;

    auto unpack = [&]<std::size_t... I>(std::index_sequence<I...>) {
      ((std::get<I>(output) = *reinterpret_cast<OutputTypes *>(outBuf.data())),
       ...);
    };
    unpack(std::make_index_sequence<sizeof...(OutputTypes)>{});
    return output;
  }
};

struct IpmiServer {
  orbis::Ref<orbis::IpmiServer> serverImpl;

  std::unordered_map<std::uint32_t,
                     std::function<orbis::ErrorCode(
                         orbis::IpmiSession &session, std::int32_t &errorCode,
                         std::vector<std::vector<std::byte>> &outData,
                         const std::vector<std::span<std::byte>> &inData)>>
      syncMethods;
  std::unordered_map<std::uint32_t,
                     std::function<orbis::ErrorCode(
                         orbis::IpmiSession &session, std::int32_t &errorCode,
                         std::vector<std::vector<std::byte>> &outData,
                         const std::vector<std::span<std::byte>> &inData)>>
      asyncMethods;
  std::vector<std::vector<std::byte>> messages;

  IpmiServer &addSyncMethodStub(
      std::uint32_t methodId,
      std::function<std::int32_t()> handler = [] -> std::int32_t {
        return 0;
      }) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (!outData.empty()) {
        return orbis::ErrorCode::INVAL;
      }

      errorCode = handler();
      return {};
    };
    return *this;
  }

  IpmiServer &addSyncMethod(
      std::uint32_t methodId,
      std::function<std::int32_t(void *out, std::uint64_t &outSize)> handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (outData.size() < 1) {
        return orbis::ErrorCode::INVAL;
      }

      std::uint64_t size = outData[0].size();
      errorCode = handler(outData[0].data(), size);
      outData[0].resize(size);
      return {};
    };
    return *this;
  }

  template <typename T>
  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<std::int32_t(void *out, std::uint64_t &outSize,
                                           const T &param)>
                    handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (outData.size() != 1 || inData.size() != 1) {
        return orbis::ErrorCode::INVAL;
      }

      if (inData[0].size() != sizeof(T)) {
        return orbis::ErrorCode::INVAL;
      }

      std::uint64_t size = outData[0].size();
      errorCode = handler(outData[0].data(), size,
                          *reinterpret_cast<T *>(inData[0].data()));
      outData[0].resize(size);
      return {};
    };
    return *this;
  }

  template <typename OutT, typename InT>
  IpmiServer &addSyncMethod(
      std::uint32_t methodId,
      std::function<std::int32_t(OutT &out, const InT &param)> handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (outData.size() != 1 || inData.size() != 1) {
        return orbis::ErrorCode::INVAL;
      }

      if (inData[0].size() != sizeof(InT)) {
        return orbis::ErrorCode::INVAL;
      }
      if (outData[0].size() < sizeof(OutT)) {
        return orbis::ErrorCode::INVAL;
      }

      OutT out;
      errorCode = handler(out, *reinterpret_cast<InT *>(inData[0].data()));
      std::memcpy(outData[0].data(), &out, sizeof(out));
      outData[0].resize(sizeof(OutT));
      return {};
    };
    return *this;
  }

  template <typename T>
  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<std::int32_t(const T &param)> handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      if (inData.size() != 1 || !outData.empty()) {
        return orbis::ErrorCode::INVAL;
      }

      if (inData[0].size() != sizeof(T)) {
        return orbis::ErrorCode::INVAL;
      }

      errorCode = handler(*reinterpret_cast<T *>(inData[0].data()));
      return {};
    };
    return *this;
  }

  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<std::int32_t(
                    std::vector<std::vector<std::byte>> &outData,
                    const std::vector<std::span<std::byte>> &inData)>
                    handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode {
      errorCode = handler(outData, inData);
      return {};
    };
    return *this;
  }

  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<orbis::ErrorCode(
                    std::vector<std::vector<std::byte>> &outData,
                    const std::vector<std::span<std::byte>> &inData)>
                    handler) {
    syncMethods[methodId] = [=](orbis::IpmiSession &session,
                                std::int32_t &errorCode,
                                std::vector<std::vector<std::byte>> &outData,
                                const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode { return handler(outData, inData); };
    return *this;
  }

  IpmiServer &
  addAsyncMethod(std::uint32_t methodId,
                 std::function<orbis::ErrorCode(
                     orbis::IpmiSession &session,
                     std::vector<std::vector<std::byte>> &outData,
                     const std::vector<std::span<std::byte>> &inData)>
                     handler) {
    asyncMethods[methodId] =
        [=](orbis::IpmiSession &session, std::int32_t &errorCode,
            std::vector<std::vector<std::byte>> &outData,
            const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode { return handler(session, outData, inData); };
    return *this;
  }

  IpmiServer &
  addSyncMethod(std::uint32_t methodId,
                std::function<orbis::ErrorCode(
                    orbis::IpmiSession &session,
                    std::vector<std::vector<std::byte>> &outData,
                    const std::vector<std::span<std::byte>> &inData)>
                    handler) {
    asyncMethods[methodId] =
        [=](orbis::IpmiSession &session, std::int32_t &errorCode,
            std::vector<std::vector<std::byte>> &outData,
            const std::vector<std::span<std::byte>> &inData)
        -> orbis::ErrorCode { return handler(session, outData, inData); };
    return *this;
  }

  template <typename T> IpmiServer &sendMsg(const T &data) {
    std::vector<std::byte> message(sizeof(T));
    std::memcpy(message.data(), &data, sizeof(T));
    messages.push_back(std::move(message));
    return *this;
  }

  orbis::ErrorCode handle(orbis::IpmiSession *session,
                          orbis::IpmiAsyncMessageHeader *message);

  orbis::ErrorCode handle(orbis::IpmiSession *session,
                          orbis::IpmiServer::Packet &packet,
                          orbis::IpmiSyncMessageHeader *message);
};

extern ipmi::IpmiClient audioIpmiClient;

IpmiClient createIpmiClient(orbis::Thread *thread, const char *name);
IpmiServer &createIpmiServer(orbis::Process *process, const char *name);
orbis::EventFlag *createEventFlag(std::string_view name, uint32_t attrs,
                                  uint64_t initPattern);
orbis::Semaphore *createSemaphore(std::string_view name, uint32_t attrs,
                                  uint64_t initCount, uint64_t maxCount);
void createShm(const char *name, uint32_t flags, uint32_t mode, uint64_t size);

void createMiniSysCoreObjects(orbis::Process *process);
void createSysAvControlObjects(orbis::Process *process);
void createAudioSystemObjects(orbis::Process *process);
void createSysCoreObjects(orbis::Process *process);
void createGnmCompositorObjects(orbis::Process *process);
void createShellCoreObjects(orbis::Process *process);
} // namespace ipmi
