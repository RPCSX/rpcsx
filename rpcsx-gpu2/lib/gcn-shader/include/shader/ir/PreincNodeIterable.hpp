#pragma once

#include "InstructionImpl.hpp" // IWYU pragma: keep

namespace shader::ir {
template <typename T> struct PreincNodeIterable {
  struct EndIterator {};

  struct Iterator {
    Instruction nextElem;
    Instruction currentElem;
    Instruction endElem;

    Iterator() = default;

    Iterator(Instruction elem, Instruction end)
        : currentElem(elem), endElem(end) {
      nextElem = currentElem ? currentElem.getNext() : nullptr;

      if constexpr (!std::is_same_v<Instruction, T>) {
        while (currentElem != endElem && !currentElem.isa<T>()) {
          advance();
        }
      }
    }

    T operator*() const { return currentElem.staticCast<T>(); }

    Iterator &operator++() {
      advance();

      if constexpr (!std::is_same_v<Instruction, T>) {
        while (currentElem != endElem && !currentElem.isa<T>()) {
          advance();
        }
      }

      return *this;
    }

    bool operator==(const Iterator &) const = default;

    bool operator==(const EndIterator &) const {
      return currentElem == endElem;
    }

    void advance() {
      currentElem = nextElem;
      if (nextElem) {
        nextElem = nextElem.getNext();
      }
    }
  };

  PreincNodeIterable(Instruction beginIt, Instruction endIt)
      : mBeginIt(beginIt), mEndIt(endIt) {}

  Iterator begin() const { return Iterator(mBeginIt, mEndIt); }
  EndIterator end() const { return EndIterator{}; }

private:
  Instruction mBeginIt;
  Instruction mEndIt;
};

template <typename T> struct RevPreincNodeIterable {
  struct EndIterator {};

  struct Iterator {
    Instruction nextElem;
    Instruction currentElem;
    Instruction endElem;

    Iterator() = default;

    Iterator(Instruction elem, Instruction end)
        : currentElem(elem), endElem(end) {
      nextElem = currentElem ? currentElem.getPrev() : nullptr;

      if constexpr (!std::is_same_v<Instruction, T>) {
        while (currentElem != endElem && !currentElem.isa<T>()) {
          advance();
        }
      }
    }

    T operator*() const { return currentElem.staticCast<T>(); }

    Iterator &operator++() {
      advance();

      if constexpr (!std::is_same_v<Instruction, T>) {
        while (currentElem != endElem && !currentElem.isa<T>()) {
          advance();
        }
      }

      return *this;
    }

    bool operator==(const Iterator &) const = default;

    bool operator==(const EndIterator &) const {
      return currentElem == endElem;
    }

    void advance() {
      currentElem = nextElem;
      if (nextElem) {
        nextElem = nextElem.getPrev();
      }
    }
  };

  RevPreincNodeIterable(Instruction beginIt, Instruction endIt)
      : mBeginIt(beginIt), mEndIt(endIt) {}

  Iterator begin() const { return Iterator(mBeginIt, mEndIt); }
  EndIterator end() const { return EndIterator{}; }

private:
  Instruction mBeginIt;
  Instruction mEndIt;
};

template <typename T = Instruction>
inline PreincNodeIterable<T> range(Instruction begin,
                                   Instruction end = nullptr) {
  return {begin, end};
}
template <typename T = Instruction>
inline RevPreincNodeIterable<T> revRange(Instruction begin,
                                         Instruction end = nullptr) {
  return {begin, end};
}
} // namespace shader::ir
