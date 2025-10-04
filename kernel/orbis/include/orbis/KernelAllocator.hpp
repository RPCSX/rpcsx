#pragma once

#include "rx/Rc.hpp"
#include <deque>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orbis {
inline namespace utils {
void *kalloc(std::size_t size, std::size_t align);
void kfree(void *ptr, std::size_t size);
template <typename T> struct kallocator {
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;

  constexpr kallocator() = default;
  template <typename U> constexpr kallocator(const kallocator<U> &) noexcept {}

  template <typename U> struct rebind {
    using other = kallocator<U>;
  };

  T *allocate(std::size_t n) {
    return static_cast<T *>(kalloc(sizeof(T) * n, alignof(T)));
  }

  void deallocate(T *p, std::size_t n) { kfree(p, sizeof(T) * n); }

  template <typename U>
  friend constexpr bool operator==(const kallocator &,
                                   const kallocator<U> &) noexcept {
    return true;
  }
};

using kstring =
    std::basic_string<char, std::char_traits<char>, kallocator<char>>;
template <typename T> using kvector = std::vector<T, kallocator<T>>;
template <typename T> using kdeque = std::deque<T, kallocator<T>>;
template <typename K, typename T, typename Cmp = std::less<>>
using kmap = std::map<K, T, Cmp, kallocator<std::pair<const K, T>>>;
template <typename K, typename T, typename Cmp = std::less<>>
using kmultimap = std::multimap<K, T, Cmp, kallocator<std::pair<const K, T>>>;
template <typename K, typename T, typename Hash = std::hash<K>,
          typename Pred = std::equal_to<K>>
using kunmap =
    std::unordered_map<K, T, Hash, Pred, kallocator<std::pair<const K, T>>>;
} // namespace utils

template <typename T, typename... Args>
  requires(std::is_constructible_v<T, Args...>)
T *knew(Args &&...args) {
  if constexpr (std::is_base_of_v<rx::RcBase, T>) {
    static_assert(!std::is_final_v<T>);
    struct DynamicObject final : T {
      using T::T;

      void operator delete(void *pointer) { utils::kfree(pointer, sizeof(T)); }
    };

    auto loc = static_cast<DynamicObject *>(
        utils::kalloc(sizeof(DynamicObject), alignof(DynamicObject)));
    return std::construct_at(loc, std::forward<Args>(args)...);
  } else {
    static_assert(!std::is_polymorphic_v<T>,
                  "Polymorphic type should be derived from rx::RcBase");

    auto loc = static_cast<T *>(utils::kalloc(sizeof(T), alignof(T)));
    return std::construct_at(loc, std::forward<Args>(args)...);
  }
}

// clang-format off
template <typename T> void kdelete(T *ptr) {
  static_assert(std::is_final_v<T>, "Uncertain type size");
  ptr->~T();
  utils::kfree(ptr, sizeof(T));
}
// clang-format on

} // namespace orbis
