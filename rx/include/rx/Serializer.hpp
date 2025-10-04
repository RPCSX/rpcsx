#pragma once

#include "rx/align.hpp"
#include "rx/refl.hpp"
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rx {
struct Serializer;
struct Deserializer;

template <typename T> struct TypeSerializer;

namespace detail {
// TODO: replace with std::is_trivially_relocatable once available
template <typename T>
concept TriviallyRelocatable =
    std::is_trivially_copyable_v<std::remove_cvref_t<T>> &&
    std::is_trivially_default_constructible_v<std::remove_cvref_t<T>> &&
    !std::is_pointer_v<T> && !std::is_reference_v<T>;

template <typename T>
concept TypeSerializable = requires(Serializer &s, const T &value) {
  TypeSerializer<std::remove_cvref_t<T>>::serialize(s, value);
} && (requires(Deserializer &d) {
  {
    TypeSerializer<std::remove_cvref_t<T>>::deserialize(d)
  } -> std::same_as<std::remove_cvref_t<T>>;
} || requires(Deserializer &d, T &value) {
  TypeSerializer<std::remove_cvref_t<T>>::deserialize(d, value);
});

template <typename T>
concept IsRange = requires(T &object) {
  object.size();
  *object.begin();
  object.begin() != object.end();
};

struct StructSerializerField {
  std::size_t offset;
  std::size_t alignment;
  std::size_t size;
  void (*serialize)(rx::Serializer &s, const void *object);
  void (*deserialize)(rx::Deserializer &s, void *object);
};

// try to call free function
template <typename T>
void callSerializeFunction(Serializer &s, const T &value)
  requires requires { serialize(s, value); }
{
  serialize(s, value);
}

// try to call free function
template <typename T>
void callDeserializeFunction(Deserializer &s, T &value)
  requires requires { deserialize(s, value); }
{
  deserialize(s, value);
}

template <typename T>
T callDeserializeFunction(Deserializer &s)
  requires requires(T &value) { deserialize<T>(s); }
{
  return deserialize<T>(s);
}

template <typename T>
concept SerializableImpl = requires(Serializer &s, const T &value) {
  value.serialize(s);
} || requires(Serializer &s, const T &value) {
  callSerializeFunction(s, value);
} || requires(Serializer &s, const T &value) {
  TypeSerializer<std::remove_cvref_t<T>>::serialize(s, value);
};

template <typename T>
concept DeserializableImpl = requires(Deserializer &d, T &value) {
  value.deserialize(d);
} || requires(Deserializer &d, T &value) {
  value = std::remove_cvref_t<T>::deserialize(d);
} || requires(Deserializer &d, T &value) {
  callDeserializeFunction(d, value);
} || requires(Deserializer &d, T &value) {
  value = callDeserializeFunction<std::remove_cvref_t<T>>(d);
} || requires(Deserializer &d) {
  {
    TypeSerializer<std::remove_cvref_t<T>>::deserialize(d)
  } -> std::same_as<std::remove_cvref_t<T>>;
} || requires(Deserializer &d, T &value) {
  TypeSerializer<std::remove_cvref_t<T>>::deserialize(d, value);
};
} // namespace detail

template <typename T>
concept Serializable =
    detail::SerializableImpl<T> && detail::DeserializableImpl<T>;

namespace detail {
struct SerializableFieldTest {
  template <Serializable FieldT>
    requires(std::is_default_constructible_v<FieldT>)
  constexpr operator FieldT();
};

struct SerializableAnyFieldTest {
  template <typename FieldT> constexpr operator FieldT();
};

template <typename T, std::size_t I> constexpr bool isSerializableField() {
  auto impl = []<std::size_t... Before, std::size_t... After>(
                  std::index_sequence<Before...>,
                  std::index_sequence<After...>) {
    return requires {
      T{(Before, SerializableAnyFieldTest{})..., SerializableFieldTest{},
        (After, SerializableAnyFieldTest{})...};
    };
  };

  return impl(std::make_index_sequence<I>{},
              std::make_index_sequence<rx::fieldCount<T> - I - 1>{});
}

template <typename T, std::size_t N = rx::fieldCount<T>>
constexpr bool isSerializableFields() {
  auto impl = []<std::size_t... I>(std::index_sequence<I...>) {
    return requires { T{(I, SerializableFieldTest{})...}; };
  };

  return impl(std::make_index_sequence<N>{});
}

template <typename T>
concept SerializableClass = !detail::IsRange<T> && std::is_class_v<T> &&
                            rx::fieldCount<T> > 0 && isSerializableFields<T>();
} // namespace detail

struct Serializer {
  virtual ~Serializer() = default;
  virtual void write(std::span<const std::byte> data) = 0;

  template <Serializable T> void serialize(const T &value) {
    if constexpr (requires { value.serialize(*this); }) {
      value.serialize(*this);
    } else if constexpr (requires { callSerializeFunction(*this, value); }) {
      callSerializeFunction(*this, value);
    } else {
      TypeSerializer<std::remove_cvref_t<T>>::serialize(*this, value);
    }
  }
};

struct Deserializer {
  virtual ~Deserializer() = default;
  virtual void read(std::span<std::byte> data) = 0;

  template <Serializable T> [[nodiscard]] std::remove_cvref_t<T> deserialize() {
    using type = std::remove_cvref_t<T>;
    if constexpr (requires {
                    { type::deserialize(*this) } -> std::convertible_to<type>;
                  }) {
      return T::deserialize(*this);
    } else if constexpr (requires(type &result) {
                           type::deserialize(*this, result);
                         }) {
      type result;
      T::deserialize(*this, result);
      return result;
    } else if constexpr (requires(type &result) {
                           {
                             result.deserialize(*this)
                           } -> std::convertible_to<type>;
                         }) {
      T result;
      result.deserialize(*this);
      return result;
    } else if constexpr (requires(type &result) {
                           type::deserialize(*this, result);
                         }) {
      type result;
      T::deserialize(*this, result);
      return result;
    } else if constexpr (requires {
                           detail::callDeserializeFunction<type>(*this);
                         }) {
      return detail::callDeserializeFunction<type>(*this);
    } else if constexpr (requires(type &value) {
                           detail::callDeserializeFunction(*this, value);
                         }) {

      type result;
      detail::callDeserializeFunction(*this, result);
      return result;
    } else if constexpr (requires {
                           TypeSerializer<type>::deserialize(*this);
                         }) {
      return TypeSerializer<type>::deserialize(*this);
    } else {
      type result;
      TypeSerializer<type>::deserialize(*this, result);
      return result;
    }
  }

  template <Serializable T> void deserialize(T &result) {
    if constexpr (requires { T::deserialize(*this, result); }) {
      T::deserialize(*this, result);
    } else if constexpr (requires { result.deserialize(*this); }) {
      result.deserialize(*this);
    } else if constexpr (requires {
                           { T::deserialize(*this) } -> std::convertible_to<T>;
                         }) {
      result = T::deserialize(*this);
    } else if constexpr (requires {
                           detail::callDeserializeFunction(*this, result);
                         }) {
      detail::callDeserializeFunction(*this, result);
    } else if constexpr (requires {
                           detail::callDeserializeFunction<T>(*this);
                         }) {
      result = detail::callDeserializeFunction<T>(*this);
    } else if constexpr (requires {
                           TypeSerializer<T>::deserialize(*this, result);
                         }) {
      TypeSerializer<T>::deserialize(*this, result);
    } else {
      result = TypeSerializer<T>::deserialize(*this);
    }
  }

  void setFailure() { mFailure = true; }
  [[nodiscard]] bool failure() const { return mFailure; }

private:
  bool mFailure = false;
};

template <detail::TriviallyRelocatable T> struct TypeSerializer<T> {
  static void serialize(Serializer &s, const T &t) {
    std::byte rawBytes[sizeof(T)];
    std::memcpy(rawBytes, &t, sizeof(T));
    s.write(rawBytes);
  }

  static T deserialize(Deserializer &s) {
    alignas(T) std::byte rawBytes[sizeof(T)];
    s.read(rawBytes);

    return std::move(std::bit_cast<T>(rawBytes));
  }
};

template <Serializable A, Serializable B>
struct TypeSerializer<std::pair<A, B>> {
  static void serialize(Serializer &s, const std::pair<A, B> &t) {
    s.serialize(t.first);
    s.serialize(t.second);
  }
  static std::pair<A, B> deserialize(Deserializer &s) {
    auto a = s.deserialize<A>();
    auto b = s.deserialize<B>();

    return {
        std::move(a),
        std::move(b),
    };
  }
};

template <Serializable... T> struct TypeSerializer<std::tuple<T...>> {
  static void serialize(Serializer &s, const std::tuple<T...> &t) {
    std::apply([&s](auto &value) { s.serialize(value); }, t);
  }

  static std::tuple<T...> deserialize(Deserializer &s) {
    return std::tuple<T...>{s.deserialize<T>()...};
  }
};

template <typename T>
  requires std::is_default_constructible_v<T> &&
           requires(Serializer &s, T &object) {
             s.serialize(object.size());
             s.serialize(*object.begin());
             object.resize(1);
             object.begin() != object.end();
           }
struct TypeSerializer<T> {
  using item_type = std::remove_cvref_t<decltype(*std::declval<T>().begin())>;

  static void serialize(Serializer &s, const T &t) {
    s.serialize(static_cast<std::uint32_t>(t.size()));

    if constexpr (detail::TriviallyRelocatable<item_type> &&
                  requires { reinterpret_cast<const std::byte *>(t.data()); }) {
      s.write({reinterpret_cast<const std::byte *>(t.data()),
               t.size() * sizeof(item_type)});
    } else {
      for (auto &item : t) {
        s.serialize(item);
      }
    }
  }

  static T deserialize(Deserializer &s) {
    auto size = s.deserialize<std::uint32_t>();

    T t;
    t.resize(size);

    if constexpr (detail::TriviallyRelocatable<item_type> &&
                  requires { reinterpret_cast<const std::byte *>(t.data()); }) {
      s.read({reinterpret_cast<std::byte *>(t.data()),
              t.size() * sizeof(item_type)});
    } else {
      for (auto &item : t) {
        s.deserialize(item);
      }
    }

    return t;
  }
};

template <detail::IsRange T>
  requires(
      std::is_default_constructible_v<T> &&
      requires(Serializer &s, T &object) {
        s.serialize(object.size());
        s.serialize(*object.begin());
        object.insert(std::move(*object.begin()));
        object.begin() != object.end();
      } && !requires(Serializer &s, T &object) { object.resize(1); })
struct TypeSerializer<T> {
  using item_type = std::remove_cvref_t<decltype(*std::declval<T>().begin())>;

  static void serialize(Serializer &s, const T &t) {
    s.serialize(static_cast<std::uint32_t>(t.size()));

    for (auto &item : t) {
      s.serialize(item);
    }
  }

  static T deserialize(Deserializer &s) {
    auto size = s.deserialize<std::uint32_t>();

    if (s.failure()) {
      return {};
    }

    T result;

    for (std::uint32_t i = 0; i < size; ++i) {
      result.insert(s.deserialize<item_type>());

      if (s.failure()) {
        return {};
      }
    }

    return result;
  }
};

namespace detail {
template <SerializableClass T> struct StructSerializerBuilder {
  static constexpr std::array<StructSerializerField, rx::fieldCount<T>>
  build() {
    StructSerializerBuilder result;

    auto impl = [&]<std::size_t... I>(std::index_sequence<I...>) {
      static_cast<void>(T{FieldVisitor{&result, I}...});
    };

    impl(std::make_index_sequence<rx::fieldCount<T>>{});

    std::size_t nextOffset = 0;
    for (auto &field : result.fields) {
      auto fieldOffset = alignUp(nextOffset, field.alignment);
      nextOffset = fieldOffset + field.size;
      field.offset = fieldOffset;
    }

    return result.fields;
  }

private:
  struct FieldVisitor {
    StructSerializerBuilder *builder;
    std::size_t fieldIndex;

    template <typename FieldT> constexpr operator FieldT() {
      builder->addField<FieldT>(fieldIndex);
      return {};
    }
  };

  template <typename FieldT> constexpr void addField(std::size_t index) {
    fields[index] = StructSerializerField{
        .offset = 0,
        .alignment = alignof(FieldT),
        .size = sizeof(FieldT),
        .serialize =
            +[](rx::Serializer &s, const void *object) {
              s.serialize(*static_cast<const FieldT *>(object));
            },
        .deserialize =
            +[](rx::Deserializer &s, void *object) {
              s.deserialize(*static_cast<FieldT *>(object));
            },
    };
  }

  std::array<StructSerializerField, fieldCount<T>> fields;
};
} // namespace detail

template <detail::SerializableClass T>
  requires(!requires {
    std::index_sequence<detail::StructSerializerBuilder<T>::build().size()>{};
  })
struct TypeSerializer<T> {
  static const auto &getFields() {
    static const auto fields = detail::StructSerializerBuilder<T>::build();
    return fields;
  }

  static void serialize(Serializer &s, const T &object) {
    s.serialize<std::uint32_t>(sizeof(object));
    auto bytes = std::bit_cast<std::byte *>(&object);
    for (auto field : getFields()) {
      field.serialize(s, bytes + field.offset);
    }
  }

  static void deserialize(Deserializer &s, T &object) {
    if (s.deserialize<std::uint32_t>() != sizeof(object)) {
      s.setFailure();
      return;
    }

    auto bytes = std::bit_cast<std::byte *>(&object);
    for (auto field : getFields()) {
      field.deserialize(s, bytes + field.offset);
      if (s.failure()) {
        return;
      }
    }
  }
};

// all fields are constructable at compile time overload
template <detail::SerializableClass T>
  requires requires {
    std::index_sequence<detail::StructSerializerBuilder<T>::build().size()>{};
  }
struct TypeSerializer<T> {
  static constexpr auto fields = detail::StructSerializerBuilder<T>::build();

  static void serialize(Serializer &s, const T &object) {
    s.serialize<std::uint32_t>(sizeof(object));
    auto bytes = std::bit_cast<std::byte *>(&object);
    for (auto field : fields) {
      field.serialize(s, bytes + field.offset);
    }
  }

  static void deserialize(Deserializer &s, T &object) {
    if (s.deserialize<std::uint32_t>() != sizeof(object)) {
      s.setFailure();
      return;
    }

    auto bytes = std::bit_cast<std::byte *>(&object);
    for (auto field : fields) {
      field.deserialize(s, bytes + field.offset);

      if (s.failure()) {
        return;
      }
    }
  }
};
} // namespace rx
