#pragma once

#include "orbis-config.hpp"
#include "rx/BitSet.hpp"
#include "rx/Rc.hpp"
#include "rx/SharedMutex.hpp"
#include <array>
#include <cstring>
#include <mutex>
#include <span>
#include <string_view>

namespace orbis {
enum class BudgetResource : uint32_t {
  Invalid,
  Dmem,
  Vmem,
  Fmem,
  CpuSet,
  File,
  Socket,
  Equeue,
  Pipe,
  Device,
  Thread,
  IpSocket,

  _count,
};

struct BudgetItem {
  uint64_t total;
  uint64_t used;
};

struct BudgetInfo {
  BudgetResource resourceId;
  uint32_t flags; // ?
  BudgetItem item;
};

static_assert(sizeof(BudgetInfo) == 0x18);

using BudgetInfoList =
    std::array<BudgetInfo, static_cast<int>(BudgetResource::_count)>;

class Budget : public rx::RcBase {
  using BudgetList =
      std::array<BudgetItem, static_cast<int>(BudgetResource::_count)>;

public:
  enum class ProcessType : orbis::uint32_t {
    BigApp,
    MiniApp,
    System,
    NonGameMiniApp,
    _last = NonGameMiniApp
  };

  Budget(std::string_view name, ProcessType pType,
         std::span<const BudgetInfo> budgets)
      : mProcessType(pType) {
    for (auto info : budgets) {
      if (info.resourceId == BudgetResource::Invalid) {
        continue;
      }

      int resourceIndex = static_cast<int>(info.resourceId);

      mUsed.set(resourceIndex);
      mList[resourceIndex] = info.item;
    }

    std::strncpy(mName, name.data(), std::min(name.size(), sizeof(mName)));
    mName[sizeof(mName) - 1] = 0;
  }

  [[nodiscard]] BudgetItem get(BudgetResource resourceId) const {
    std::lock_guard lock(mMtx);
    return mList[static_cast<int>(resourceId)];
  }

  [[nodiscard]] BudgetList getBudgetList() const {
    std::lock_guard lock(mMtx);
    return mList;
  }

  [[nodiscard]] std::pair<BudgetInfoList, int> getList() const {
    auto budgetList = getBudgetList();

    BudgetInfoList result{};
    int count = 0;

    for (auto resourceId : mUsed) {
      result[count].resourceId = static_cast<BudgetResource>(resourceId);
      result[count].item = budgetList[resourceId];
      count++;
    }

    return {result, count};
  }

  bool acquire(BudgetResource resourceId, std::uint64_t size = 1) {
    auto &budget = mList[static_cast<int>(resourceId)];

    std::lock_guard lock(mMtx);

    if (budget.used + size > budget.total) {
      return false;
    }

    budget.used += size;
    return true;
  }

  void release(BudgetResource resourceId, std::uint64_t size) {
    auto &budget = mList[static_cast<int>(resourceId)];

    std::lock_guard lock(mMtx);

    if (size >= budget.used) {
      budget.used = 0;
    } else {
      budget.used -= size;
    }
  }

  bool hasResource(BudgetResource resourceId) const {
    return mUsed.test(static_cast<int>(resourceId));
  }

  [[nodiscard]] int size() const { return mUsed.popcount(); }
  [[nodiscard]] ProcessType processType() const { return mProcessType; }

private:
  mutable rx::shared_mutex mMtx;
  rx::BitSet<static_cast<int>(BudgetResource::_count)> mUsed;
  ProcessType mProcessType{};
  BudgetList mList;
  char mName[32]{};
};
} // namespace orbis
