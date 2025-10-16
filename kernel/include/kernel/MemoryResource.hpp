#pragma once

#include "rx/AddressRange.hpp"
#include "rx/Mappable.hpp"
#include "rx/MemoryTable.hpp"
#include "rx/Serializer.hpp"
#include "rx/align.hpp"
#include <concepts>
#include <cstddef>
#include <iterator>
#include <system_error>

namespace kernel {
enum class AllocationFlags : std::uint8_t {
  NoOverwrite,
  NoMerge,
  Fixed,
  Stack,
  Dry,

  bitset_last = Fixed
};

template <typename Resource> struct MappableResource {
  std::size_t size;
  rx::Mappable mappable;

  void serialize(rx::Serializer &s) const {
    s.serialize(size);

    // FIXME: serialize memory
  }

  void deserialize(rx::Deserializer &s) {
    auto _size = s.deserialize<std::size_t>();

    if (create(_size) != std::errc{}) {
      s.setFailure();
      return;
    }

    // FIXME: deserialize memory
  }

  std::errc create(std::size_t newSize) {
    auto [_mappable, _errc] = Resource{}(newSize);

    if (_errc != std::errc{}) {
      return _errc;
    }

    size = newSize;
    mappable = std::move(_mappable);
    return {};
  }

  void destroy() {
    size = 0;
    mappable = {};
  }
};

struct ExternalResource {
  std::errc create(std::size_t) { return {}; }
  void serialize(rx::Serializer &) const {}
  void deserialize(rx::Deserializer &) {}
};

template <typename T>
concept AllocationInfo = requires(T &t, rx::AddressRange range) {
  { t.isAllocated() } -> std::convertible_to<bool>;
  { t.isRelated(t, range, range) } -> std::convertible_to<bool>;
  { t.merge(t, range, range) } -> std::convertible_to<T>;
  requires rx::Serializable<T>;
};

template <AllocationInfo AllocationT,
          rx::Serializable Resource = ExternalResource>
struct AllocableResource : Resource {
  mutable rx::MemoryTableWithPayload<AllocationT> allocations;
  using iterator = typename rx::MemoryTableWithPayload<AllocationT>::iterator;

  struct AllocationResult {
    iterator it;
    std::errc errc;
    rx::AddressRange range;
  };

  void serialize(rx::Serializer &s) const {
    Resource::serialize(s);

    for (auto a : allocations) {
      s.serialize(a.beginAddress());
      s.serialize(a.endAddress());
      s.serialize(a.get());
    }

    s.serialize<std::uint64_t>(-1);
    s.serialize<std::uint64_t>(-1);
  }

  void deserialize(rx::Deserializer &s) {
    Resource::deserialize(s);

    while (true) {
      auto beginAddress = s.deserialize<std::uint64_t>();
      auto endAddress = s.deserialize<std::uint64_t>();

      if (s.failure()) {
        return;
      }

      if (beginAddress == endAddress &&
          beginAddress == static_cast<std::uint64_t>(-1)) {
        break;
      }

      auto range = rx::AddressRange::fromBeginEnd(beginAddress, endAddress);

      if (!range) {
        s.setFailure();
        return;
      }

      auto allocation = s.deserialize<AllocationT>();

      if (s.failure()) {
        return;
      }

      allocations.map(range, std::move(allocation));
    }
  }

  iterator query(std::uint64_t address) {
    return allocations.queryArea(address);
  }

  std::errc create(rx::AddressRange range) {
    auto errc = Resource::create(range.size());
    if (errc != std::errc{}) {
      return errc;
    }

    allocations.map(range, {});
    return {};
  }

  iterator begin() { return allocations.begin(); }
  iterator end() { return allocations.end(); }

  AllocationResult map(std::uint64_t addressHint, std::uint64_t size,
                            AllocationT &allocationInfo,
                            rx::EnumBitSet<AllocationFlags> flags,
                            std::uint64_t alignment) {
    if (flags & AllocationFlags::Stack) {
      addressHint = rx::alignDown(addressHint, alignment);
    } else {
      addressHint = rx::alignUp(addressHint, alignment);
    }

    auto it = allocations.queryArea(addressHint);
    if (it == allocations.end()) {
      return {end(), std::errc::invalid_argument, {}};
    }

    rx::AddressRange fixedRange;

    if (flags & AllocationFlags::Fixed) {
      // fixed allocation, replace everything

      fixedRange = rx::AddressRange::fromBeginSize(addressHint, size);
      iterator endIt = allocations.queryArea(addressHint + size - 1);

      if (endIt == end()) {
        // out of reserved space
        return {end(), std::errc::not_enough_memory, {}};
      }

      if (flags & AllocationFlags::NoOverwrite) {
        if (it->isAllocated() || !it.range().contains(fixedRange)) {
          return {end(), std::errc::invalid_argument, {}};
        }
      } else if ((flags & AllocationFlags::Dry) != AllocationFlags::Dry) {
        if constexpr (requires {
                        it->truncate(it.range(), it.range(), it.range());
                      }) {
          // allocation info listens truncations, notify all affected nodes

          auto notifyTruncation = [this](iterator node,
                                         rx::AddressRange leftRange,
                                         rx::AddressRange rightRange) {
            if (!node->isAllocated()) {
              // ignore changes of not allocated nodes
              return;
            }

            auto [newRange, payload] =
                node->truncate(node.range(), leftRange, rightRange);

            if (newRange.isValid()) {
              // truncation produces new node, apply changes
              allocations.map(newRange, std::move(payload));
            }
          };

          if (it == endIt) {
            // allocation changes single region, merge truncations
            notifyTruncation(
                it,
                rx::AddressRange::fromBeginEnd(it.range().beginAddress(),
                                               fixedRange.beginAddress()),
                rx::AddressRange::fromBeginEnd(fixedRange.endAddress(),
                                               it.range().endAddress()));
          } else {
            auto beginIt = it;

            // first region can be truncated right
            notifyTruncation(beginIt,
                             rx::AddressRange::fromBeginEnd(
                                 beginIt.beginAddress(), addressHint),
                             rx::AddressRange{});

            ++beginIt;

            while (beginIt != endIt) {
              // all allocations between begin and end region will be removed
              notifyTruncation(beginIt, rx::AddressRange{}, rx::AddressRange{});
              ++beginIt;
            }

            // last region can be left truncated or removed
            notifyTruncation(endIt, rx::AddressRange{},
                             rx::AddressRange::fromBeginEnd(
                                 addressHint + size, endIt.endAddress()));
          }
        }
      }
    } else {
      auto hasEnoughSpace = [alignment, size](rx::AddressRange range) {
        auto alignedAddress = rx::AddressRange::fromBeginEnd(
            rx::alignUp(range.beginAddress(), alignment), range.endAddress());

        return alignedAddress.isValid() && alignedAddress.size() >= size;
      };

      if (addressHint != 0 && (flags & AllocationFlags::Stack)) {
        while (it != begin()) {
          if (!it->isAllocated() && hasEnoughSpace(it.range())) {
            break;
          }

          --it;
        }

        if (it->isAllocated() || !hasEnoughSpace(it.range())) {
          return {end(), std::errc::not_enough_memory, {}};
        }
      } else {
        while (it != end()) {
          if (!it->isAllocated() && hasEnoughSpace(it.range())) {
            break;
          }

          ++it;
        }

        if (it == end()) {
          return {end(), std::errc::not_enough_memory, {}};
        }
      }

      // now `it` points to region that meets requirements, create fixed range
      fixedRange = rx::AddressRange::fromBeginEnd(
          rx::alignUp(it->beginAddress(), alignment), it->endAddress());
    }

    if (flags & AllocationFlags::Dry) {
      return {end(), {}, fixedRange};
    }

    if (it.range() == fixedRange) {
      // we have exact mapping already, just replace payload
      it.get() = std::move(allocationInfo);
    } else {
      it = allocations.map(fixedRange, std::move(allocationInfo), false);
    }

    if (flags & AllocationFlags::NoMerge) {
      // if we shouldn't merge related allocations, allocations map already
      // valid
      return {it, {}, fixedRange};
    }

    if (it != begin()) {
      // try to merge with previous node
      iterator prevIt = std::prev(it);
      if (prevIt->isAllocated() &&
          prevIt->isRelated(it.get(), prevIt.range(), it.range())) {
        // previous block is allocated and related to current block, do merge
        auto mergedRange = rx::AddressRange::fromBeginEnd(prevIt.beginAddress(),
                                                          it.endAddress());
        it = allocations.map(
            mergedRange, prevIt->merge(it.get(), prevIt.range(), it.range()));
      }
    }

    if (iterator nextIt = std::next(it); nextIt != end()) {
      if (nextIt->isAllocated() &&
          it->isRelated(nextIt.get(), it.range(), nextIt.range())) {
        // next block is allocated and related to current block, do merge
        auto mergedRange = rx::AddressRange::fromBeginEnd(it.beginAddress(),
                                                          nextIt.endAddress());
        it = allocations.map(
            mergedRange, it->merge(nextIt.get(), it.range(), nextIt.range()));
      }
    }

    return {it, {}, fixedRange};
  }

  void destroy() {
    allocations.clear();
    Resource::destroy();
  }
};
} // namespace kernel
