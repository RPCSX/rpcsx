#pragma once
#include "cf.hpp"
#include <amdgpu/RemoteMemory.hpp>

namespace amdgpu::shader {
cf::BasicBlock *buildCf(cf::Context &ctxt, RemoteMemory memory,
                        std::uint64_t entryPoint);
} // namespace amdgpu::shader
