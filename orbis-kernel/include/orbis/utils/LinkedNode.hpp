#pragma once

namespace orbis {
inline namespace utils {
template <typename T> struct LinkedNode {
  T object;
  LinkedNode *next = nullptr;
  LinkedNode *prev = nullptr;

  void insertNext(LinkedNode &other) {
    other.next = next;
    other.prev = this;

    if (next != nullptr) {
      next->prev = &other;
    }

    next = &other;
  }

  void insertPrev(LinkedNode &other) {
    other.next = this;
    other.prev = prev;

    if (prev != nullptr) {
      prev->next = &other;
    }

    prev = &other;
  }

  LinkedNode *erase() {
    if (prev != nullptr) {
      prev->next = next;
    }

    if (next != nullptr) {
      next->prev = prev;
    }

    prev = nullptr;
    auto result = next;
    next = nullptr;
    return result;
  }
};
} // namespace utils
} // namespace orbis
