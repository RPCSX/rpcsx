#pragma once

#include "BitSet.hpp"
#include "Rc.hpp"
#include "orbis/utils/SharedMutex.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace orbis {
inline namespace utils {
template <WithRc T, typename IdT = int, std::size_t MaxId = 4096,
          std::size_t MinId = 0>
  requires(MaxId > MinId)
class RcIdMap {
  static constexpr auto ChunkSize = std::min<std::size_t>(MaxId - MinId, 64);
  static constexpr auto ChunkCount =
      (MaxId - MinId + ChunkSize - 1) / ChunkSize;

  struct IdMapChunk {
    BitSet<ChunkSize> mask = {};
    T *objects[ChunkSize]{};

    ~IdMapChunk() {
      std::size_t index = mask.countr_zero();

      while (index < ChunkSize) {
        objects[index]->decRef();
        index = mask.countr_zero(index + 1);
      }
    }

    std::size_t insert(T *object) {
      std::size_t index = mask.countr_one();
      mask.set(index);
      objects[index] = object;

      return index;
    }

    T *get(std::size_t index) { return objects[index]; }

    void remove(std::size_t index) {
      objects[index]->decRef();
      objects[index] = nullptr;
      mask.clear(index);
    }
  };

  IdMapChunk m_chunks[ChunkCount]{};
  BitSet<ChunkCount> m_fullChunks;

public:
  static constexpr auto npos = static_cast<IdT>(~static_cast<std::size_t>(0));

  shared_mutex mutex;

  struct end_iterator {};

  class iterator {
    const IdMapChunk *chunks = nullptr;
    std::size_t chunk = 0;
    std::size_t index = 0;

  public:
    iterator(const IdMapChunk *chunks) : chunks(chunks) { findNext(); }

    iterator &operator++() {
      ++index;
      findNext();
      return *this;
    }

    std::pair<IdT, T *> operator*() const {
      return {static_cast<IdT>(chunk * ChunkSize + index + MinId),
              chunks[chunk].objects[index]};
    }

    bool operator!=(const end_iterator &) const { return chunk < ChunkCount; }
    bool operator==(const end_iterator &) const { return chunk >= ChunkCount; }

  private:
    void findNext() {
      while (chunk < ChunkCount) {
        index = chunks[chunk].mask.countr_zero(index);

        if (index < ChunkSize) {
          break;
        }

        index = 0;
        chunk++;
      }
    }
  };

  void walk(auto cb) {
    std::lock_guard lock(mutex);

    for (std::size_t chunk = 0; chunk < ChunkCount; ++chunk) {
      std::size_t index = m_chunks[chunk].mask.countr_zero();

      while (index < ChunkSize) {
        cb(static_cast<IdT>(index + chunk * ChunkSize + MinId),
           m_chunks[chunk].objects[index]);

        index = m_chunks[chunk].mask.countr_zero(index + 1);
      }
    }
  }

  iterator begin() const { return iterator{m_chunks}; }

  end_iterator end() const { return {}; }

private:
  IdT insert_impl(T *object) {
    std::lock_guard lock(mutex);

    auto page = m_fullChunks.countr_one();

    if (page == ChunkCount) {
      return npos;
    }

    auto index = m_chunks[page].insert(object);

    if (m_chunks[page].mask.full()) {
      m_fullChunks.set(page);
    }

    return {static_cast<IdT>(page * ChunkSize + index + MinId)};
  }

public:
  IdT insert(T *object) {
    auto result = insert_impl(object);

    if (result != npos) {
      object->incRef();
    }

    return result;
  }

  IdT insert(const Ref<T> &ref) { return insert(ref.get()); }

  IdT insert(Ref<T> &&ref) {
    auto object = ref.release();
    auto result = insert_impl(object);

    if (result == npos) {
      object->decRef();
    }

    return result;
  }

  T *get(IdT id) {
    const auto rawId = static_cast<std::size_t>(id) - MinId;

    if (rawId >= MaxId - MinId) {
      return nullptr;
    }

    const auto chunk = rawId / ChunkSize;
    const auto index = rawId % ChunkSize;

    std::lock_guard lock(mutex);

    if (!m_chunks[chunk].mask.test(index)) {
      return nullptr;
    }

    return m_chunks[chunk].get(index);
  }

  bool destroy(IdT id)
    requires requires(T t) { t.destroy(); }
  {
    const auto rawId = static_cast<std::size_t>(id) - MinId;

    if (rawId >= MaxId - MinId) {
      return false;
    }

    const auto chunk = rawId / ChunkSize;
    const auto index = rawId % ChunkSize;

    std::lock_guard lock(mutex);

    if (!m_chunks[chunk].mask.test(index)) {
      return false;
    }

    m_chunks[chunk].get(index)->destroy();
    m_chunks[chunk].remove(index);
    m_fullChunks.clear(chunk);
    return true;
  }

  bool close(IdT id) {
    const auto rawId = static_cast<std::size_t>(id) - MinId;

    if (rawId >= MaxId - MinId) {
      return false;
    }

    const auto chunk = rawId / ChunkSize;
    const auto index = rawId % ChunkSize;

    std::lock_guard lock(mutex);

    if (!m_chunks[chunk].mask.test(index)) {
      return false;
    }

    m_chunks[chunk].remove(index);
    m_fullChunks.clear(chunk);
    return true;
  }

  [[deprecated("use close()")]] bool remove(IdT id) { return close(id); }
};

template <typename T, typename IdT = int, std::size_t MaxId = 4096,
          std::size_t MinId = 0>
  requires(MaxId > MinId)
struct OwningIdMap {
  static constexpr auto ChunkSize = std::min<std::size_t>(MaxId - MinId, 64);
  static constexpr auto ChunkCount =
      (MaxId - MinId + ChunkSize - 1) / ChunkSize;

  struct IdMapChunk {
    BitSet<ChunkSize> mask = {};
    alignas(T) std::byte objects[sizeof(T) * ChunkSize];

    ~IdMapChunk() {
      std::size_t pageOffset = 0;

      for (auto page : mask._bits) {
        auto tmp = page;

        while (true) {
          const auto index = std::countr_zero(tmp);

          if (index >= 64) {
            break;
          }

          tmp &= ~(static_cast<std::uint64_t>(1) << index);
          destroy(pageOffset + index);
        }

        pageOffset += 64;
      }
    }

    template <typename... ArgsT>
    std::pair<std::size_t, T *> emplace_new(ArgsT &&...args) {
      std::size_t index = mask.countr_one();

      if (index >= ChunkSize) {
        return {};
      }

      mask.set(index);

      return {index,
              std::construct_at(get(index), std::forward<ArgsT>(args)...)};
    }

    T *get(std::size_t index) {
      return reinterpret_cast<T *>(objects + sizeof(T) * index);
    }

    void destroy(std::size_t index) {
      std::destroy_at(get(index));
      mask.clear(index);
    }
  };

  IdMapChunk chunks[ChunkCount]{};
  BitSet<ChunkCount> fullChunks;

  template <typename... ArgsT>
    requires(std::is_constructible_v<T, ArgsT...>)
  std::pair<IdT, T *> emplace(ArgsT &&...args) {
    auto page = fullChunks.countr_one();

    if (page == ChunkCount) {
      return {};
    }

    auto newElem = chunks[page].emplace_new(std::forward<ArgsT>(args)...);

    if (chunks[page].mask.full()) {
      fullChunks.set(page);
    }

    return {static_cast<IdT>(page * ChunkSize + newElem.first + MinId),
            newElem.second};
  }

  T *get(IdT id) {
    const auto rawId = static_cast<std::size_t>(id) - MinId;
    const auto chunk = rawId / ChunkSize;
    const auto index = rawId % ChunkSize;

    if (chunk >= ChunkCount) {
      return nullptr;
    }

    if (!chunks[chunk].mask.test(index)) {
      return nullptr;
    }

    return chunks[chunk].get(index);
  }

  bool destroy(IdT id) {
    const auto rawId = static_cast<std::size_t>(id) - MinId;
    const auto chunk = rawId / ChunkSize;
    const auto index = rawId % ChunkSize;

    if (chunk >= ChunkCount) {
      return false;
    }

    if (!chunks[chunk].mask.test(index)) {
      return false;
    }

    chunks[chunk].destroy(index);
    fullChunks.clear(chunk);
    return true;
  }
};
} // namespace utils
} // namespace orbis
