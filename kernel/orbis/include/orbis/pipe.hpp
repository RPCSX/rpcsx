#pragma once

#include "KernelAllocator.hpp"
#include "file.hpp"
#include "rx/Rc.hpp"
#include "rx/SharedCV.hpp"
#include "rx/SharedMutex.hpp"
#include <utility>

namespace orbis {
struct Pipe : File {
  rx::shared_cv cv;
  kvector<std::byte> data;
  rx::Ref<Pipe> other;
};

std::pair<rx::Ref<Pipe>, rx::Ref<Pipe>> createPipe();
} // namespace orbis
