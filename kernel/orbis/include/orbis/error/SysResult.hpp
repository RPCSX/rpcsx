#pragma once
#include <compare>

namespace orbis {
enum class ErrorCode : int;

class SysResult {
  int mValue = 0;

public:
  SysResult() = default;
  SysResult(ErrorCode ec) : mValue(-static_cast<int>(ec)) {}

  [[nodiscard]] static SysResult notAnError(ErrorCode ec) {
    SysResult result;
    result.mValue = static_cast<int>(ec);
    return result;
  }

  [[nodiscard]] int value() const { return mValue < 0 ? -mValue : mValue; }
  [[nodiscard]] bool isError() const { return mValue < 0; }

  [[nodiscard]] auto operator<=>(ErrorCode ec) const {
    return static_cast<ErrorCode>(value()) <=> ec;
  }

  [[nodiscard]] auto operator<=>(SysResult other) const {
    return value() <=> other.value();
  }
};
} // namespace orbis
