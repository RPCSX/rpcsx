#pragma once
#include "file.hpp"
#include "note.hpp"
#include "utils/SharedCV.hpp"
#include <list>

namespace orbis {
struct KQueue : orbis::File {
  shared_cv cv;
  std::list<KNote, kallocator<KNote>> notes;
};
} // namespace orbis
