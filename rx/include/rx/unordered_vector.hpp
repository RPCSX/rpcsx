#pragma once

#include <cstddef>
#include <utility>

namespace rx {
template <typename VectorT>
void unordered_vector_erase(VectorT &vector, std::size_t pos) {
  if (pos + 1 == vector.size()) {
    vector.pop_back();
    return;
  }

  std::swap(vector[pos], vector.back());
  vector.pop_back();
}

template <typename VectorT, typename Value>
void unordered_vector_insert(VectorT &vector, std::size_t pos, Value &&value) {
  bool isLast = pos + 1 == vector.size();
  vector.push_back(std::move(value));

  if (isLast) {
    return;
  }

  std::swap(vector[pos], vector.back());
}
} // namespace rx
