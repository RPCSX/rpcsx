#pragma once

namespace util {
class SourceLocation {
public:
  const char *mFileName = {};
  const char *mFunctionName = {};
  unsigned mLine = 0;
  unsigned mColumn = 0;

public:
  constexpr SourceLocation(const char *fileName = __builtin_FILE(),
                           const char *functionName = __builtin_FUNCTION(),
                           unsigned line = __builtin_LINE(),
                           unsigned column =
#if __has_builtin(__builtin_COLUMN)
                               __builtin_COLUMN()
#else
                               0
#endif
                               ) noexcept
      : mFileName(fileName), mFunctionName(functionName), mLine(line),
        mColumn(column) {
  }

  constexpr unsigned line() const noexcept { return mLine; }
  constexpr unsigned column() const noexcept { return mColumn; }
  constexpr const char *file_name() const noexcept { return mFileName; }
  constexpr const char *function_name() const noexcept { return mFunctionName; }
};
} // namespace util
