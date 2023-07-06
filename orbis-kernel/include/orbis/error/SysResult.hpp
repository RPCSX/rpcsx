#pragma once

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
};
} // namespace orbis
