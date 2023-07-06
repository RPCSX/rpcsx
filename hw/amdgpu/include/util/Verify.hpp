#pragma once

#include "SourceLocation.hpp"
#include "unreachable.hpp"

class Verify {
  util::SourceLocation mLocation;

public:
  util::SourceLocation location() const { return mLocation; }

  Verify(util::SourceLocation location = util::SourceLocation())
      : mLocation(location) {}

  Verify &operator<<(bool result) {
    if (!result) {
      util::unreachable("Verification failed at %s: %s:%u:%u",
                        mLocation.function_name(), mLocation.file_name(),
                        mLocation.line(), mLocation.column());
    }

    return *this;
  }
};
