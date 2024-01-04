#pragma once

#include "KernelAllocator.hpp"
#include "file.hpp"
#include "utils/Rc.hpp"
#include "utils/SharedCV.hpp"
#include "utils/SharedMutex.hpp"
#include <utility>

namespace orbis {
struct Pipe final : File {
  shared_cv cv;
  kvector<std::byte> data;
  Ref<Pipe> other;
};

std::pair<Ref<Pipe>, Ref<Pipe>> createPipe();
} // namespace orbis
