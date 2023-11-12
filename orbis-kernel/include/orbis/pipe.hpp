#pragma once

#include "KernelAllocator.hpp"
#include "file.hpp"
#include "utils/Rc.hpp"

namespace orbis {
struct Pipe final : File {
  kvector<std::byte> data;
};

Ref<Pipe> createPipe();
} // namespace orbis
