#pragma once

#include "InstructionImpl.hpp" // IWYU pragma: keep
#include <type_traits>

namespace shader::ir {
template <typename T = Instruction> struct Range {
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

  Range(Instruction beginIt, Instruction endIt)
      : mBeginIt(beginIt), mEndIt(endIt) {}

  template <typename OtherT>
    requires(!std::is_same_v<OtherT, Range>)
  Range(OtherT other) : Range(other.mBeginIt, other.mEndIt) {}

  Iterator begin() const { return Iterator(mBeginIt, mEndIt); }
  EndIterator end() const { return EndIterator{}; }

private:
  Instruction mBeginIt;
  Instruction mEndIt;

  template <typename> friend struct Range;
};

template <typename T = Instruction> struct RevRange {
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

  RevRange(Instruction beginIt, Instruction endIt)
      : mBeginIt(beginIt), mEndIt(endIt) {}

  template <typename OtherT>
    requires(!std::is_same_v<OtherT, RevRange>)
  RevRange(OtherT other) : RevRange(other.mBeginIt, other.mEndIt) {}

  Iterator begin() const { return Iterator(mBeginIt, mEndIt); }
  EndIterator end() const { return EndIterator{}; }

private:
  Instruction mBeginIt;
  Instruction mEndIt;

  template <typename> friend struct RevRange;
};

template <typename T = Instruction>
inline Range<T> range(Instruction begin, Instruction end = nullptr) {
  if (end) {
    assert(begin.getParent() == end.getParent());
  }

  return {begin, end};
}
template <typename T = Instruction>
inline RevRange<T> revRange(Instruction begin, Instruction end = nullptr) {
  if (end) {
    assert(begin.getParent() == end.getParent());
  }

  return {begin, end};
}
} // namespace shader::ir
