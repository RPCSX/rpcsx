#pragma once
#include "ir/Location.hpp"
#include "spv.hpp"
#include <filesystem>

namespace shader::glsl {
enum class Stage {
  Library,
  Vertex,
  TessControl,
  TessEvaluation,
  Geometry,
  Fragment,
  Compute,
  RayGen,
  Intersect,
  AnyHit,
  ClosestHit,
  Miss,
  Callable,
  Task,
  Mesh,
};

std::optional<spv::BinaryLayout> parseFile(ir::Context &context, Stage stage,
                                           const std::filesystem::path &path);
std::optional<spv::BinaryLayout> parseSource(ir::Context &context, Stage stage,
                                             std::string_view source,
                                             ir::Location loc = nullptr);
std::string decompile(std::span<const std::uint32_t> spv);
} // namespace shader::glsl
