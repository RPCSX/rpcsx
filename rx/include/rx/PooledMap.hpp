#pragma once

#include <iterator>
#include <map>

namespace rx {
template <typename KT, typename VT, typename Compare, typename Alloc>
class PooledMap {
  std::map<KT, VT, Compare, Alloc> mMap;
  std::multimap<KT, VT, Compare, Alloc> mFreeNodes;

public:
  using node_type = decltype(mMap)::node_type;
  using insert_return_type = decltype(mMap)::insert_return_type;
  using iterator = decltype(mMap)::iterator;
  using const_iterator = decltype(mMap)::const_iterator;
  using key_compare = decltype(mMap)::key_compare;

  void clear() {
    for (auto it = begin(); it != end(); it = erase(it)) {
    }
  }

  [[nodiscard]] std::size_t size() const { return mMap.size(); }
  [[nodiscard]] std::size_t capacity() const { return mFreeNodes.size(); }

  void reserve(std::size_t size) const {
    if (mMap.size() >= size) {
      return;
    }

    auto growSize = size - mMap.size();
    auto cap = capacity();

    if (growSize <= cap) {
      return;
    }

    growSize -= cap;
    grow(growSize);
  }

  [[nodiscard]] bool empty() const { return mMap.empty(); }

  template <typename T> VT &operator[](T &&key) {
    auto it = mMap.lower_bound(key);

    if (it == mMap.end() || key_comp()(key, it->first)) {
      auto result = mMap.insert(it, createNode(std::forward<T>(key), {}));
      return result->second;
    }

    return it->second;
  }

  template <typename T, typename U>
  std::pair<iterator, bool> insert(T &&key, U &&value) {
    auto it = mMap.lower_bound(key);

    if (it == mMap.end() || key_comp()(key, it->first)) {
      it = mMap.insert(
          it, createNode(std::forward<T>(key), std::forward<U>(value)));

      return {it, true};
    }

    return {it, false};
  }

  template <typename T, typename U>
  iterator insert(const_iterator hint, T &&key, U &&value) {
    return mMap.insert(
        hint, createNode(std::forward<T>(key), std::forward<U>(value)));
  }

  [[nodiscard]] key_compare key_comp() const { return mMap.key_comp(); }

  template <typename T> iterator find(const T &key) { return mMap.find(key); }
  template <typename T> const_iterator find(const T &key) const {
    return mMap.find(key);
  }

  template <typename T> iterator lower_bound(const T &key) {
    return mMap.lower_bound(key);
  }
  template <typename T> const_iterator lower_bound(const T &key) const {
    return mMap.lower_bound(key);
  }

  template <typename T> iterator upper_bound(const T &key) {
    return mMap.upper_bound(key);
  }
  template <typename T> const_iterator upper_bound(const T &key) const {
    return mMap.upper_bound(key);
  }

  template <typename T> bool contains(const T &key) const {
    return find(key) != mMap.end();
  }

  template <typename T> iterator erase(const T &key) {
    return erase(find(key));
  }

  iterator erase(iterator it) {
    auto result = std::next(it);
    auto node = mMap.extract(it);
    node.key() = KT();
    node.mapped() = VT();

    mFreeNodes.insert(std::move(node));
    return result;
  }

  const_iterator erase(const_iterator it) {
    auto result = std::next(it);
    auto node = mMap.extract(it);
    node.key() = KT();
    node.mapped() = VT();

    mFreeNodes.insert(std::move(node));
    return result;
  }

  void grow(std::size_t count) {
    while (count-- > 0) {
      mFreeNodes.insert(std::pair{KT{}, VT{}});
    }
  }

  [[nodiscard]] iterator begin() { return mMap.begin(); }
  [[nodiscard]] iterator end() { return mMap.end(); }
  [[nodiscard]] const_iterator begin() const { return mMap.begin(); }
  [[nodiscard]] const_iterator end() const { return mMap.end(); }
  [[nodiscard]] const_iterator cbegin() const { return mMap.cbegin(); }
  [[nodiscard]] const_iterator cend() const { return mMap.cend(); }

private:
  node_type createNode(KT key, VT mapped) {
    if (mFreeNodes.empty()) {
      mFreeNodes.insert(std::pair{std::move(key), std::move(mapped)});
      return mFreeNodes.extract(mFreeNodes.begin());
    }

    auto result = mFreeNodes.extract(mFreeNodes.begin());
    result.key() = std::move(key);
    result.mapped() = std::move(mapped);
    return result;
  }
};
} // namespace rx
