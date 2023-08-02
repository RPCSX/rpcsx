#pragma once

#include "utils/Rc.hpp"
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
  auto loc = static_cast<T *>(utils::kalloc(sizeof(T), alignof(T)));
  auto res = std::construct_at(loc, std::forward<Args>(args)...);
  if constexpr (requires(T *t) { t->_total_size = sizeof(T); })
    res->_total_size = sizeof(T);
  return res;
}

// clang-format off
template <typename T> void kdelete(T *ptr) {
  auto total_size = sizeof(T);
  if constexpr (requires(T *t) { t->_total_size = sizeof(T); })
    total_size = ptr->_total_size;
  else
    static_assert(std::is_final_v<T>, "Uncertain type size");
  ptr->~T();
  utils::kfree(ptr, total_size);
}
// clang-format on

} // namespace orbis
