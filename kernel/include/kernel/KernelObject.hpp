#pragma once

#include "rx/Rc.hpp"
#include "rx/Serializer.hpp"
#include "rx/SharedMutex.hpp"
#include "rx/TypeId.hpp"
#include <cstddef>
#include <memory>
#include <mutex>

namespace kernel {
class KernelObjectBase : public rx::RcBase {
  rx::TypeId mType;

public:
  KernelObjectBase(rx::TypeId type) : mType(type) {}
  [[nodiscard]] rx::TypeId getType() const { return mType; }

  template <typename T> [[nodiscard]] bool isa() const {
    return mType == rx::TypeId::get<T>();
  }
};

template <rx::Serializable StateT>
struct KernelObject : KernelObjectBase, StateT {
  template <typename... Args>
  KernelObject(Args &&...args)
      : KernelObjectBase(rx::TypeId::get<StateT>()),
        StateT(std::forward<Args>(args)...) {}

  virtual void serialize(rx::Serializer &s) const {
    if constexpr (requires(const KernelObject &instance) {
                    instance.lock();
                    instance.unlock();
                  }) {
      std::lock_guard lock(*this);
      s.serialize(static_cast<const StateT &>(*this));
    } else {
      s.serialize(static_cast<const StateT &>(*this));
    }
  }

  virtual void deserialize(rx::Deserializer &s) {
    s.deserialize(static_cast<StateT &>(*this));
  }
};

template <rx::Serializable StateT>
struct LockableKernelObject : KernelObjectBase, StateT {
  mutable rx::shared_mutex mtx;

  template <typename... Args>
  LockableKernelObject(Args &&...args)
      : KernelObjectBase(rx::TypeId::get<StateT>()),
        StateT(std::forward<Args>(args)...) {}

  virtual void serialize(rx::Serializer &s) const {
    std::lock_guard lock(*this);
    s.serialize(static_cast<const StateT &>(*this));
  }

  virtual void deserialize(rx::Deserializer &s) {
    s.deserialize(static_cast<StateT &>(*this));
  }

  void lock() const { mtx.lock(); }
  void unlock() const { mtx.unlock(); }
  bool try_lock() const { return mtx.try_lock(); }
};

namespace detail {
struct StaticObjectCtl {
  std::size_t offset = -1ull;
  void (*construct)(void *object);
  void (*destruct)(void *object);
  void (*serialize)(void *object, rx::Serializer &);
  void (*deserialize)(void *object, rx::Deserializer &);

  template <typename T> constexpr static StaticObjectCtl Create() {
    return {
        .construct =
            +[](void *object) {
              std::construct_at(reinterpret_cast<T *>(object));
            },
        .destruct = +[](void *object) { reinterpret_cast<T *>(object)->~T(); },
        .serialize =
            +[](void *object, rx::Serializer &serializer) {
              serializer.serialize(*reinterpret_cast<T *>(object));
            },
        .deserialize =
            +[](void *object, rx::Deserializer &deserializer) {
              deserializer.deserialize(*reinterpret_cast<T *>(object));
            },
    };
  }
};

struct GlobalScope;
struct ProcessScope;
struct ThreadScope;
} // namespace detail

template <typename NamespaceT, typename ScopeT>
struct StaticKernelObjectStorage {
  template <typename T> static std::uint32_t Allocate() {
    auto &instance = GetInstance();

    auto object = detail::StaticObjectCtl::Create<T>();
    instance.m_registry.push_back(object);

    auto offset = instance.m_size;
    offset = rx::alignUp(offset, alignof(T));
    instance.m_registry.back().offset = offset;
    instance.m_size = offset + sizeof(T);
    instance.m_alignment =
        std::max<std::size_t>(alignof(T), instance.m_alignment);
    return offset;
  }

  static std::size_t GetSize() { return GetInstance().m_size; }
  static std::size_t GetAlignment() { return GetInstance().m_alignment; }

  static void ConstructAll(std::byte *storage) {
    auto &instance = GetInstance();

    for (auto objectCtl : instance.m_registry) {
      objectCtl.construct(storage + objectCtl.offset);
    }
  }

  static void DestructAll(std::byte *storage) {
    auto &instance = GetInstance();

    for (auto objectCtl : instance.m_registry) {
      objectCtl.destruct(storage + objectCtl.offset);
    }
  }

  static void SerializeAll(std::byte *storage, rx::Serializer &s) {
    auto &instance = GetInstance();

    s.serialize(instance.m_size);
    s.serialize(instance.m_registry.size());

    for (auto objectCtl : instance.m_registry) {
      objectCtl.serialize(storage + objectCtl.offset, s);
    }
  }

  static void DeserializeAll(std::byte *storage, rx::Deserializer &s) {
    auto &instance = GetInstance();

    auto size = s.deserialize<std::size_t>();
    auto registrySize = s.deserialize<std::size_t>();

    if (size != instance.m_size || registrySize != instance.m_registry.size()) {
      s.setFailure();
      return;
    }

    for (auto objectCtl : instance.m_registry) {
      objectCtl.deserialize(storage + objectCtl.offset, s);
    }
  }

private:
  static StaticKernelObjectStorage &GetInstance() {
    static StaticKernelObjectStorage instance;
    return instance;
  }

  std::vector<detail::StaticObjectCtl> m_registry;
  std::size_t m_size = 0;
  std::size_t m_alignment = 1;
};

template <typename Namespace, typename Scope, rx::Serializable T>
struct StaticObjectRef {
  using type = T;
  std::uint32_t offset;

  T *get(std::byte *storage) { return reinterpret_cast<T *>(storage + offset); }
};

} // namespace kernel
