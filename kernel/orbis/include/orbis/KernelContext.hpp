#pragma once
#include "AppInfo.hpp"
#include "Budget.hpp"
#include "IoDevice.hpp"
#include "KernelObject.hpp"
#include "evf.hpp"
#include "ipmi.hpp"
#include "orbis/note.hpp"
#include "osem.hpp"
#include "rx/IdMap.hpp"
#include "rx/SharedMutex.hpp"

#include <cstdint>
#include <mutex>
#include <pthread.h>
#include <span>
#include <utility>

namespace orbis {
struct Process;
struct Thread;

enum class FwType : std::uint8_t {
  Unknown,
  Ps4,
  Ps5,
};

struct RcAppInfo : rx::RcBase, AppInfoEx {
  orbis::uint32_t appState = 0;
};

class KernelContext final {
public:
  KernelContext();
  ~KernelContext();

  long getTscFreq();

  std::pair<EventFlag *, bool> createEventFlag(kstring name, std::int32_t flags,
                                               std::uint64_t initPattern) {
    std::lock_guard lock(m_evf_mtx);

    auto [it, inserted] = m_event_flags.try_emplace(std::move(name), nullptr);
    if (inserted) {
      it->second = knew<EventFlag>(flags, initPattern);
      std::strncpy(it->second->name, it->first.c_str(), 32);
    }

    return {it->second.get(), inserted};
  }

  rx::Ref<EventFlag> findEventFlag(std::string_view name) {
    std::lock_guard lock(m_evf_mtx);

    if (auto it = m_event_flags.find(name); it != m_event_flags.end()) {
      return it->second;
    }

    return {};
  }

  std::pair<Semaphore *, bool> createSemaphore(kstring name,
                                               std::uint32_t attrs,
                                               std::int32_t initCount,
                                               std::int32_t maxCount) {
    std::lock_guard lock(m_sem_mtx);
    auto [it, inserted] = m_semaphores.try_emplace(std::move(name), nullptr);
    if (inserted) {
      it->second = knew<Semaphore>(attrs, initCount, maxCount);
    }

    return {it->second.get(), inserted};
  }

  rx::Ref<Semaphore> findSemaphore(std::string_view name) {
    std::lock_guard lock(m_sem_mtx);
    if (auto it = m_semaphores.find(name); it != m_semaphores.end()) {
      return it->second;
    }

    return {};
  }

  std::pair<rx::Ref<IpmiServer>, ErrorCode> createIpmiServer(kstring name) {
    std::lock_guard lock(m_sem_mtx);
    auto [it, inserted] = mIpmiServers.try_emplace(std::move(name), nullptr);

    if (!inserted) {
      return {it->second, ErrorCode::EXIST};
    }

    it->second = knew<IpmiServer>(it->first);

    if (it->second == nullptr) {
      mIpmiServers.erase(it);
      return {nullptr, ErrorCode::NOMEM};
    }

    return {it->second, {}};
  }

  rx::Ref<IpmiServer> findIpmiServer(std::string_view name) {
    std::lock_guard lock(m_sem_mtx);
    if (auto it = mIpmiServers.find(name); it != mIpmiServers.end()) {
      return it->second;
    }

    return {};
  }

  std::tuple<kmap<kstring, rx::StaticString<127>> &, std::unique_lock<rx::shared_mutex>>
  getKernelEnv() {
    std::unique_lock lock(m_kenv_mtx);
    return {m_kenv, std::move(lock)};
  }

  void setKernelEnv(std::string_view key, std::string_view value) {
    std::unique_lock lock(m_kenv_mtx);
    m_kenv[kstring(key)] = value;
  }

  rx::Ref<EventEmitter> deviceEventEmitter;
  rx::Ref<IoDevice> shmDevice;
  rx::Ref<File> dmem;
  rx::Ref<File> blockpool;
  rx::Ref<rx::RcBase> gpuDevice;
  rx::Ref<IoDevice> dceDevice;
  rx::shared_mutex gpuDeviceMtx;
  uint sdkVersion{};
  uint fwSdkVersion{};
  uint safeMode{};
  rx::RcIdMap<rx::RcBase, sint, 4097, 1> ipmiMap;
  rx::RcIdMap<RcAppInfo> appInfos;
  rx::RcIdMap<Budget, sint, 4097, 1> budgets;
  rx::Ref<Budget> processTypeBudgets[4];

  rx::shared_mutex regMgrMtx;
  kmap<std::uint32_t, std::uint32_t> regMgrInt;
  kmap<std::uint32_t, rx::StaticCString<2048>> regMgrStr;
  std::vector<std::tuple<std::uint8_t *, size_t>> dialogs{};

  FwType fwType = FwType::Unknown;
  bool isDevKit = false;

  rx::Ref<Budget> createProcessTypeBudget(Budget::ProcessType processType,
                                          std::string_view name,
                                          std::span<const BudgetInfo> items) {
    auto budget = orbis::knew<orbis::Budget>(name, processType, items);
    processTypeBudgets[static_cast<int>(processType)] =
        orbis::knew<orbis::Budget>(name, processType, items);
    return budget;
  }

  rx::Ref<Budget> getProcessTypeBudget(Budget::ProcessType processType) {
    return processTypeBudgets[static_cast<int>(processType)];
  }

  void serialize(rx::Serializer &) const {}
  void deserialize(rx::Deserializer &) {}

private:
  std::atomic<long> m_tsc_freq{0};

  rx::shared_mutex m_evf_mtx;
  kmap<kstring, rx::Ref<EventFlag>> m_event_flags;

  rx::shared_mutex m_sem_mtx;
  kmap<kstring, rx::Ref<Semaphore>> m_semaphores;

  rx::shared_mutex mIpmiServerMtx;
  kmap<kstring, rx::Ref<IpmiServer>> mIpmiServers;

  rx::shared_mutex m_kenv_mtx;
  kmap<kstring, rx::StaticString<127>> m_kenv;
};

extern GlobalObjectRef<KernelContext> g_context;
} // namespace orbis
