#pragma once

#include <utility>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include "utils/Rc.hpp"

namespace orbis {
inline namespace utils {
void *kalloc(std::size_t size, std::size_t align);
void kfree(void *ptr, std::size_t size);
template <typename T> struct kallocator {
  using value_type = T;

  template <typename U> struct rebind { using other = kallocator<U>; };

  T *allocate(std::size_t n) {
    return static_cast<T *>(kalloc(sizeof(T) * n, alignof(T)));
  }

  void deallocate(T *p, std::size_t n) {
    kfree(p, sizeof(T) * n);
  }

  template <typename U>
  friend constexpr bool operator==(const kallocator &,
                                   const kallocator<U> &) noexcept {
    return true;
  }
};

template <typename T> using kvector = std::vector<T, kallocator<T>>;
template <typename T> using kdeque = std::deque<T, kallocator<T>>;
template <typename K, typename T, typename Cmp = std::less<>>
using kmap = std::map<K, T, Cmp, kallocator<std::pair<const K, T>>>;
template <typename K, typename T, typename Hash = std::hash<K>,
  typename Pred = std::equal_to<K>>
  using kunmap =
  std::unordered_map<K, T, Hash, Pred, kallocator<std::pair<const K, T>>>;
} // namespace utils

template <typename T, typename... Args> T *knew(Args &&...args) {
  auto loc = static_cast<T *>(utils::kalloc(sizeof(T), alignof(T)));
  auto res = std::construct_at(loc, std::forward<Args>(args)...);
  if constexpr (WithRc<T>)
    res->_total_size = sizeof(T);
  return res;
}

template <typename T> void kdelete(T *ptr) {
  ptr->~T();
  utils::kfree(ptr, sizeof(T));
}

} // namespace orbis
