#pragma once

#include "rx/AddressRange.hpp"
#include "rx/Rc.hpp"
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <utility>

namespace rx {
struct AreaInfo {
  std::uint64_t beginAddress;
  std::uint64_t endAddress;
};

struct NoInvalidationHandle {
  void handleInvalidation(std::uint64_t) {}
};

struct StdSetInvalidationHandle {
  std::set<std::uint64_t, std::greater<>> invalidated;

  void handleInvalidation(std::uint64_t address) {
    invalidated.insert(address);
  }
};

template <typename InvalidationHandleT = NoInvalidationHandle>
class MemoryAreaTable : public InvalidationHandleT {
  enum class Kind { O, X };
  std::map<std::uint64_t, Kind> mAreas;

public:
  class iterator {
    using map_iterator = typename std::map<std::uint64_t, Kind>::iterator;
    map_iterator it;

  public:
    iterator() = default;
    iterator(map_iterator it) : it(it) {}

    AreaInfo operator*() const { return {it->first, std::next(it)->first}; }

    iterator &operator++() {
      ++it;
      ++it;
      return *this;
    }

    iterator &operator--() {
      --it;
      --it;
      return *this;
    }

    bool operator==(iterator other) const { return it == other.it; }
    bool operator!=(iterator other) const { return it != other.it; }
  };

  iterator begin() { return iterator(mAreas.begin()); }
  iterator end() { return iterator(mAreas.end()); }

  void clear() { mAreas.clear(); }

  [[nodiscard]] AreaInfo queryArea(std::uint64_t address) const {
    auto it = mAreas.lower_bound(address);
    assert(it != mAreas.end());
    std::uint64_t endAddress = 0;
    if (it->first != address) {
      assert(it->second == Kind::X);
      endAddress = it->first;
      --it;
    } else {
      assert(it->second == Kind::O);
      endAddress = std::next(it)->first;
    }

    auto startAddress = std::uint64_t(it->first);

    return {startAddress, endAddress};
  }

  void map(rx::AddressRange range) {
    auto [beginIt, beginInserted] =
        mAreas.emplace(range.beginAddress(), Kind::O);
    auto [endIt, endInserted] = mAreas.emplace(range.endAddress(), Kind::X);

    if (!beginInserted) {
      if (beginIt->second == Kind::X) {
        // it was close, extend to open
        assert(beginIt != mAreas.begin());
        --beginIt;
      }
    } else if (beginIt != mAreas.begin()) {
      auto prevRangePointIt = std::prev(beginIt);

      if (prevRangePointIt->second == Kind::O) {
        // we found range start before inserted one, remove insertion and extend
        // begin
        this->handleInvalidation(beginIt->first);
        mAreas.erase(beginIt);
        beginIt = prevRangePointIt;
      }
    }

    if (!endInserted) {
      if (endIt->second == Kind::O) {
        // it was open, extend to close
        assert(endIt != mAreas.end());
        ++endIt;
      }
    } else {
      auto nextRangePointIt = std::next(endIt);

      if (nextRangePointIt != mAreas.end() &&
          nextRangePointIt->second == Kind::X) {
        // we found range end after inserted one, remove insertion and extend
        // end
        this->handleInvalidation(std::prev(endIt)->first);
        mAreas.erase(endIt);
        endIt = nextRangePointIt;
      }
    }

    // eat everything in middle of the range
    ++beginIt;
    while (beginIt != endIt) {
      this->handleInvalidation(std::prev(endIt)->first);
      beginIt = mAreas.erase(beginIt);
    }
  }

  void unmap(std::uint64_t beginAddress, std::uint64_t endAddress) {
    auto beginIt = mAreas.lower_bound(beginAddress);

    if (beginIt == mAreas.end()) {
      return;
    }
    if (beginIt->first >= endAddress) {
      if (beginIt->second != Kind::X) {
        return;
      }

      auto prevEnd = beginIt->first;

      --beginIt;
      if (beginIt->first >= endAddress) {
        return;
      }

      if (beginIt->first < beginAddress) {
        this->handleInvalidation(beginIt->first);
        beginIt = mAreas.emplace(beginAddress, Kind::X).first;
      }

      if (prevEnd > endAddress) {
        mAreas.emplace(endAddress, Kind::O);
        return;
      }
    }

    if (beginIt->first > beginAddress && beginIt->second == Kind::X) {
      // we have found end after unmap begin, need to insert new end
      this->handleInvalidation(std::prev(beginIt)->first);
      auto newBeginIt = mAreas.emplace_hint(beginIt, beginAddress, Kind::X);
      mAreas.erase(beginIt);

      if (newBeginIt == mAreas.end()) {
        return;
      }

      beginIt = std::next(newBeginIt);
    } else if (beginIt->second == Kind::X) {
      beginIt = ++beginIt;
    }

    Kind lastKind = Kind::X;
    while (beginIt != mAreas.end() && beginIt->first <= endAddress) {
      lastKind = beginIt->second;
      if (lastKind == Kind::O) {
        this->handleInvalidation(std::prev(beginIt)->first);
      }
      beginIt = mAreas.erase(beginIt);
    }

    if (lastKind != Kind::O) {
      return;
    }

    // Last removed was range open, need to insert new one at unmap end
    mAreas.emplace_hint(beginIt, endAddress, Kind::O);
  }

  std::size_t totalMemory() const {
    std::size_t result = 0;

    for (auto it = mAreas.begin(), end = mAreas.end(); it != end; ++it) {
      auto rangeBegin = it;
      auto rangeEnd = ++it;

      result += rangeEnd->first - rangeBegin->first;
    }

    return result;
  }
};

template <typename T> class Payload {
  enum class Kind { O, X, XO };
  Kind kind;

  union Storage {
    T data;
    Storage() {}
    ~Storage() {}
  } storage;

  template <typename... Args>
    requires std::constructible_from<T, Args &&...>
  Payload(Kind kind, Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>)
      : kind(kind) {
    std::construct_at(&storage.data, std::forward<Args>(args)...);
  }

public:
  ~Payload() noexcept(std::is_nothrow_destructible_v<T>) {
    if (kind != Kind::X) {
      storage.data.~T();
    }
  }

  Payload(Payload &&other) noexcept(std::is_nothrow_move_constructible_v<T>)
      : kind(other.kind) {
    if (!isClose()) {
      std::construct_at(&storage.data, std::move(other.storage.data));
    }
  }

  Payload &
  operator=(Payload &&other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                      std::is_nothrow_move_assignable_v<T>) {
    if (this == &other) {
      return *this;
    }

    if (other.isClose()) {
      if (!isClose()) {
        storage.data.~T();
        kind = other.kind;
      }

      return *this;
    }

    if (!isClose()) {
      storage.data = std::move(other.storage.data);
    } else {
      std::construct_at(&storage.data, std::move(other.storage.data));
    }

    kind = other.kind;
    return *this;
  }

  T &get() {
    assert(kind != Kind::X);
    return storage.data;
  }
  const T &get() const {
    assert(kind != Kind::X);
    return storage.data;
  }

  T exchange(T data) {
    assert(kind != Kind::X);
    return std::exchange(storage.data, data);
  }

  template <typename... Args>
    requires std::constructible_from<T, Args...>
  [[nodiscard]] static Payload createOpen(Args &&...args) {
    return Payload(Kind::O, std::forward<Args>(args)...);
  }

  template <typename... Args>
    requires std::constructible_from<T, Args...>
  [[nodiscard]] static Payload createClose(Args &&...args) {
    return Payload(Kind::X, std::forward<Args>(args)...);
  }

  template <typename... Args>
    requires std::constructible_from<T, Args...>
  [[nodiscard]] static Payload createCloseOpen(Args &&...args) {
    return Payload(Kind::XO, std::forward<Args>(args)...);
  }

  [[nodiscard]] bool isOpen() const { return kind == Kind::O; }
  [[nodiscard]] bool isClose() const { return kind == Kind::X; }
  [[nodiscard]] bool isCloseOpen() const { return kind == Kind::XO; }

  void setCloseOpen() {
    assert(kind != Kind::X);
    kind = Kind::XO;
  }
  void setOpen() {
    assert(kind != Kind::X);
    kind = Kind::O;
  }
};

template <typename T> class Payload<T *> {
  static constexpr std::uintptr_t kCloseOpenBit =
      alignof(T) > 1 ? 1 : (1ull << (sizeof(std::uintptr_t) * 8 - 1));
  static constexpr std::uintptr_t kClose = 0;
  std::uintptr_t value = kClose;

public:
  T *get() const {
    assert(!isClose());
    return std::bit_cast<T *>(value & ~kCloseOpenBit);
  }

  T *exchange(T *data) {
    assert(!isClose());
    auto result = get();
    value = std::bit_cast<std::uintptr_t>(data) | (value & kCloseOpenBit);
    return result;
  }

  [[nodiscard]] static Payload createOpen(T *ptr) {
    Payload result;
    result.value = std::bit_cast<std::uintptr_t>(ptr);
    return result;
  }

  template <typename... Args> [[nodiscard]] static Payload createClose() {
    return Payload();
  }

  [[nodiscard]] static Payload createCloseOpen(T *ptr) {
    Payload result;
    result.value = std::bit_cast<std::uintptr_t>(ptr) | kCloseOpenBit;
    return result;
  }

  [[nodiscard]] bool isOpen() const {
    return value != kClose && (value & kCloseOpenBit) == 0;
  }
  [[nodiscard]] bool isClose() const { return value == kClose; }
  [[nodiscard]] bool isCloseOpen() const {
    return (value & kCloseOpenBit) != 0;
  }

  void setCloseOpen() {
    assert(!isClose());
    value |= kCloseOpenBit;
  }

  void setOpen() {
    assert(!isClose());
    value &= ~kCloseOpenBit;
  }
};

template <typename T> class Payload<Ref<T>> {
  static constexpr std::uintptr_t kCloseOpenBit =
      alignof(T) > 1 ? 1 : (1ull << (sizeof(std::uintptr_t) * 8 - 1));
  static constexpr std::uintptr_t kClose = 0;
  std::uintptr_t value = kClose;

public:
  ~Payload() noexcept(std::is_nothrow_destructible_v<T>) {
    if (!isClose()) {
      get()->decRef();
    }
  }

  Payload(Payload &&other) noexcept(std::is_nothrow_destructible_v<T>) {
    if (!isClose()) {
      get()->decRef();
    }

    value = other.value;
    other.value = kClose;
  }

  Payload &
  operator=(Payload &&other) noexcept(std::is_nothrow_destructible_v<T>) {
    if (this == &other) {
      return *this;
    }

    if (!isClose()) {
      get()->decRef();
    }

    value = other.value;
    other.value = kClose;
    return *this;
  }

  T *get() const {
    assert(!isClose());
    return std::bit_cast<T *>(value & ~kCloseOpenBit);
  }

  T *exchange(T *data) {
    assert(!isClose());
    auto result = get();
    value = std::bit_cast<std::uintptr_t>(data) | (value & kCloseOpenBit);
    result->decRef();
    return result;
  }

  [[nodiscard]] static Payload createOpen(T *ptr) {
    Payload result;
    result.value = std::bit_cast<std::uintptr_t>(ptr);
    ptr->incRef();
    return result;
  }

  template <typename... Args> [[nodiscard]] static Payload createClose() {
    return Payload();
  }

  [[nodiscard]] static Payload createCloseOpen(T *ptr) {
    Payload result;
    result.value = std::bit_cast<std::uintptr_t>(ptr) | kCloseOpenBit;
    ptr->incRef();
    return result;
  }

  [[nodiscard]] bool isOpen() const {
    return value != kClose && (value & kCloseOpenBit) == 0;
  }
  [[nodiscard]] bool isClose() const { return value == kClose; }
  [[nodiscard]] bool isCloseOpen() const {
    return (value & kCloseOpenBit) != 0;
  }

  void setCloseOpen() {
    assert(!isClose());
    value |= kCloseOpenBit;
  }

  void setOpen() {
    assert(!isClose());
    value &= ~kCloseOpenBit;
  }
};

template <typename PayloadT,
          template <typename> typename Allocator = std::allocator>
class MemoryTableWithPayload {
  using payload_type = Payload<PayloadT>;
  std::map<std::uint64_t, payload_type, std::less<>,
           Allocator<std::pair<const std::uint64_t, payload_type>>>
      mAreas;

public:
  class AreaInfo : public rx::AddressRange {
    Payload<PayloadT> &payload;

  public:
    AreaInfo(Payload<PayloadT> &payload, rx::AddressRange range)
        : payload(payload), AddressRange(range) {}

    decltype(auto) operator->() { return &payload.get(); }
    decltype(auto) get() { return payload.get(); }
  };

  class iterator {
    using map_iterator =
        typename std::map<std::uint64_t, payload_type>::iterator;
    map_iterator it;

  public:
    iterator() = default;
    iterator(map_iterator it) : it(it) {}

    AreaInfo operator*() const { return {it->second, range()}; }

    rx::AddressRange range() const {
      return rx::AddressRange::fromBeginEnd(beginAddress(), endAddress());
    }

    std::uint64_t beginAddress() const { return it->first; }
    std::uint64_t endAddress() const { return std::next(it)->first; }
    std::uint64_t size() const { return endAddress() - beginAddress(); }

    decltype(auto) get() const { return it->second.get(); }
    decltype(auto) operator->() const { return &it->second.get(); }
    iterator &operator++() {
      ++it;

      if (!it->second.isCloseOpen()) {
        ++it;
      }

      return *this;
    }

    bool operator==(iterator other) const { return it == other.it; }
    bool operator!=(iterator other) const { return it != other.it; }

    friend MemoryTableWithPayload;
  };

  MemoryTableWithPayload() = default;
  MemoryTableWithPayload(MemoryTableWithPayload &&) = default;
  MemoryTableWithPayload &operator=(MemoryTableWithPayload &&) = default;
  MemoryTableWithPayload(const MemoryTableWithPayload &) = delete;
  MemoryTableWithPayload &operator=(const MemoryTableWithPayload &) = delete;

  iterator begin() { return iterator(mAreas.begin()); }
  iterator end() { return iterator(mAreas.end()); }

  void clear() { mAreas.clear(); }

  iterator lowerBound(std::uint64_t address) {
    auto it = mAreas.lower_bound(address);

    if (it == mAreas.end()) {
      return it;
    }

    if (it->first == address) {
      if (it->second.isClose()) {
        ++it;
      }
    } else {
      if (!it->second.isOpen()) {
        --it;
      }
    }

    return it;
  }

  iterator queryArea(std::uint64_t address) {
    auto it = mAreas.lower_bound(address);

    if (it == mAreas.end()) {
      return it;
    }

    std::uint64_t endAddress = 0;

    if (it->first == address) {
      if (it->second.isClose()) {
        return mAreas.end();
      }

      endAddress = std::next(it)->first;
    } else {
      if (it->second.isOpen()) {
        return mAreas.end();
      }

      endAddress = it->first;
      --it;
    }

    return endAddress < address ? mAreas.end() : it;
  }

  iterator map(rx::AddressRange range, PayloadT payload, bool merge = true,
               bool noOverride = false) {
    assert(range.beginAddress() < range.endAddress());
    auto [beginIt, beginInserted] =
        mAreas.emplace(range.beginAddress(), payload_type::createOpen(payload));
    auto [endIt, endInserted] =
        mAreas.emplace(range.endAddress(), payload_type::createClose());

    bool seenOpen = false;
    bool endCollision = false;
    bool lastRemovedIsOpen = false;
    PayloadT lastRemovedOpenPayload;
    if (noOverride && !beginInserted && !endInserted &&
        std::next(beginIt) == endIt) {
      return beginIt;
    }

    if (!beginInserted || !endInserted) {
      if (!beginInserted) {
        if (beginIt->second.isClose()) {
          beginIt->second = payload_type::createCloseOpen(payload);
        } else {
          seenOpen = true;
          lastRemovedIsOpen = true;
          lastRemovedOpenPayload = beginIt->second.exchange(std::move(payload));
        }
      }

      if (!endInserted) {
        if (endIt->second.isOpen()) {
          endIt->second.setCloseOpen();
        } else {
          endCollision = true;
        }

        lastRemovedIsOpen = false;
      }
    } else if (beginIt != mAreas.begin()) {
      auto prev = std::prev(beginIt);

      if (!prev->second.isClose()) {
        beginIt->second.setCloseOpen();
        seenOpen = true;
        lastRemovedIsOpen = true;
        lastRemovedOpenPayload = prev->second.get();
      }
    }

    auto origBegin = beginIt;
    ++beginIt;
    while (beginIt != endIt) {
      if (beginIt->second.isClose()) {
        lastRemovedIsOpen = false;
        if (!seenOpen) {
          origBegin->second.setCloseOpen();
        }
      } else {
        if (!seenOpen && beginIt->second.isCloseOpen()) {
          origBegin->second.setCloseOpen();
        }

        seenOpen = true;
        lastRemovedIsOpen = true;
        lastRemovedOpenPayload = std::move(beginIt->second.get());
      }
      beginIt = mAreas.erase(beginIt);
    }

    if (endCollision && !seenOpen) {
      origBegin->second.setCloseOpen();
    } else if (lastRemovedIsOpen && !endCollision) {
      endIt->second =
          payload_type::createCloseOpen(std::move(lastRemovedOpenPayload));
    }

    if (!merge) {
      return origBegin;
    }

    if (origBegin->second.isCloseOpen()) {
      auto prevBegin = std::prev(origBegin);

      if (prevBegin->second.get() == origBegin->second.get()) {
        mAreas.erase(origBegin);
        origBegin = prevBegin;
      }
    }

    if (endIt->second.isCloseOpen()) {
      if (endIt->second.get() == origBegin->second.get()) {
        mAreas.erase(endIt);
      }
    }

    return origBegin;
  }

  void unmap(iterator it) {
    auto openIt = it.it;
    auto closeIt = openIt;
    ++closeIt;

    if (openIt->second.isCloseOpen()) {
      openIt->second = payload_type::createClose();
    } else {
      mAreas.erase(openIt);
    }

    if (closeIt->second.isCloseOpen()) {
      closeIt->second.setOpen();
    } else {
      mAreas.erase(closeIt);
    }
  }

  void unmap(rx::AddressRange range) {
    // FIXME: can be optimized
    unmap(map(range, PayloadT{}, false));
  }
};
} // namespace rx
