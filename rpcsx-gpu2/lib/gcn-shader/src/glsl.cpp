#include "glsl.hpp"
#include "SPIRV/GlslangToSpv.h"
#include "dialect/spv.hpp"
#include "spv.hpp"
#include <filesystem>
#include <fstream>
#include <glslang/Public/ShaderLang.h>
#include <spirv_cross_c.h>

static constexpr auto g_glslangLimit = 100;

static constexpr TBuiltInResource g_glslangConfig = {
    .maxLights = g_glslangLimit,
    .maxClipPlanes = g_glslangLimit,
    .maxTextureUnits = g_glslangLimit,
    .maxTextureCoords = g_glslangLimit,
    .maxVertexAttribs = g_glslangLimit,
    .maxVertexUniformComponents = g_glslangLimit,
    .maxVaryingFloats = g_glslangLimit,
    .maxVertexTextureImageUnits = g_glslangLimit,
    .maxCombinedTextureImageUnits = g_glslangLimit,
    .maxTextureImageUnits = g_glslangLimit,
    .maxFragmentUniformComponents = g_glslangLimit,
    .maxDrawBuffers = g_glslangLimit,
    .maxVertexUniformVectors = g_glslangLimit,
    .maxVaryingVectors = g_glslangLimit,
    .maxFragmentUniformVectors = g_glslangLimit,
    .maxVertexOutputVectors = g_glslangLimit,
    .maxFragmentInputVectors = g_glslangLimit,
    .minProgramTexelOffset = g_glslangLimit,
    .maxProgramTexelOffset = g_glslangLimit,
    .maxClipDistances = g_glslangLimit,
    .maxComputeWorkGroupCountX = g_glslangLimit,
    .maxComputeWorkGroupCountY = g_glslangLimit,
    .maxComputeWorkGroupCountZ = g_glslangLimit,
    .maxComputeWorkGroupSizeX = g_glslangLimit,
    .maxComputeWorkGroupSizeY = g_glslangLimit,
    .maxComputeWorkGroupSizeZ = g_glslangLimit,
    .maxComputeUniformComponents = g_glslangLimit,
    .maxComputeTextureImageUnits = g_glslangLimit,
    .maxComputeImageUniforms = g_glslangLimit,
    .maxComputeAtomicCounters = g_glslangLimit,
    .maxComputeAtomicCounterBuffers = g_glslangLimit,
    .maxVaryingComponents = g_glslangLimit,
    .maxVertexOutputComponents = g_glslangLimit,
    .maxGeometryInputComponents = g_glslangLimit,
    .maxGeometryOutputComponents = g_glslangLimit,
    .maxFragmentInputComponents = g_glslangLimit,
    .maxImageUnits = g_glslangLimit,
    .maxCombinedImageUnitsAndFragmentOutputs = g_glslangLimit,
    .maxCombinedShaderOutputResources = g_glslangLimit,
    .maxImageSamples = g_glslangLimit,
    .maxVertexImageUniforms = g_glslangLimit,
    .maxTessControlImageUniforms = g_glslangLimit,
    .maxTessEvaluationImageUniforms = g_glslangLimit,
    .maxGeometryImageUniforms = g_glslangLimit,
    .maxFragmentImageUniforms = g_glslangLimit,
    .maxCombinedImageUniforms = g_glslangLimit,
    .maxGeometryTextureImageUnits = g_glslangLimit,
    .maxGeometryOutputVertices = g_glslangLimit,
    .maxGeometryTotalOutputComponents = g_glslangLimit,
    .maxGeometryUniformComponents = g_glslangLimit,
    .maxGeometryVaryingComponents = g_glslangLimit,
    .maxTessControlInputComponents = g_glslangLimit,
    .maxTessControlOutputComponents = g_glslangLimit,
    .maxTessControlTextureImageUnits = g_glslangLimit,
    .maxTessControlUniformComponents = g_glslangLimit,
    .maxTessControlTotalOutputComponents = g_glslangLimit,
    .maxTessEvaluationInputComponents = g_glslangLimit,
    .maxTessEvaluationOutputComponents = g_glslangLimit,
    .maxTessEvaluationTextureImageUnits = g_glslangLimit,
    .maxTessEvaluationUniformComponents = g_glslangLimit,
    .maxTessPatchComponents = g_glslangLimit,
    .maxPatchVertices = g_glslangLimit,
    .maxTessGenLevel = g_glslangLimit,
    .maxViewports = g_glslangLimit,
    .maxVertexAtomicCounters = g_glslangLimit,
    .maxTessControlAtomicCounters = g_glslangLimit,
    .maxTessEvaluationAtomicCounters = g_glslangLimit,
    .maxGeometryAtomicCounters = g_glslangLimit,
    .maxFragmentAtomicCounters = g_glslangLimit,
    .maxCombinedAtomicCounters = g_glslangLimit,
    .maxAtomicCounterBindings = g_glslangLimit,
    .maxVertexAtomicCounterBuffers = g_glslangLimit,
    .maxTessControlAtomicCounterBuffers = g_glslangLimit,
    .maxTessEvaluationAtomicCounterBuffers = g_glslangLimit,
    .maxGeometryAtomicCounterBuffers = g_glslangLimit,
    .maxFragmentAtomicCounterBuffers = g_glslangLimit,
    .maxCombinedAtomicCounterBuffers = g_glslangLimit,
    .maxAtomicCounterBufferSize = g_glslangLimit,
    .maxTransformFeedbackBuffers = g_glslangLimit,
    .maxTransformFeedbackInterleavedComponents = g_glslangLimit,
    .maxCullDistances = g_glslangLimit,
    .maxCombinedClipAndCullDistances = g_glslangLimit,
    .maxSamples = g_glslangLimit,
    .maxMeshOutputVerticesNV = g_glslangLimit,
    .maxMeshOutputPrimitivesNV = g_glslangLimit,
    .maxMeshWorkGroupSizeX_NV = g_glslangLimit,
    .maxMeshWorkGroupSizeY_NV = g_glslangLimit,
    .maxMeshWorkGroupSizeZ_NV = g_glslangLimit,
    .maxTaskWorkGroupSizeX_NV = g_glslangLimit,
    .maxTaskWorkGroupSizeY_NV = g_glslangLimit,
    .maxTaskWorkGroupSizeZ_NV = g_glslangLimit,
    .maxMeshViewCountNV = g_glslangLimit,
    .maxMeshOutputVerticesEXT = g_glslangLimit,
    .maxMeshOutputPrimitivesEXT = g_glslangLimit,
    .maxMeshWorkGroupSizeX_EXT = g_glslangLimit,
    .maxMeshWorkGroupSizeY_EXT = g_glslangLimit,
    .maxMeshWorkGroupSizeZ_EXT = g_glslangLimit,
    .maxTaskWorkGroupSizeX_EXT = g_glslangLimit,
    .maxTaskWorkGroupSizeY_EXT = g_glslangLimit,
    .maxTaskWorkGroupSizeZ_EXT = g_glslangLimit,
    .maxMeshViewCountEXT = g_glslangLimit,
    .maxDualSourceDrawBuffersEXT = g_glslangLimit,

    .limits = {
        .nonInductiveForLoops = true,
        .whileLoops = true,
        .doWhileLoops = true,
        .generalUniformIndexing = true,
        .generalAttributeMatrixVectorIndexing = true,
        .generalVaryingIndexing = true,
        .generalSamplerIndexing = true,
        .generalVariableIndexing = true,
        .generalConstantMatrixVectorIndexing = true,
    }};

static std::optional<std::vector<std::byte>>
readFile(const std::filesystem::path &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);

  if (!f) {
    return {};
  }

  std::vector<std::byte> data(f.tellg());
  f.seekg(0, std::ios::beg);
  f.read(reinterpret_cast<char *>(data.data()), data.size());
  return data;
}

static EShLanguage toGlslangStage(shader::glsl::Stage stage) {
  using shader::glsl::Stage;
  switch (stage) {
  case Stage::Library:
    return EShLangCompute;
  case Stage::Vertex:
    return EShLangVertex;
  case Stage::TessControl:
    return EShLangTessControl;
  case Stage::TessEvaluation:
    return EShLangTessEvaluation;
  case Stage::Geometry:
    return EShLangGeometry;
  case Stage::Fragment:
    return EShLangFragment;
  case Stage::Compute:
    return EShLangCompute;
  case Stage::RayGen:
    return EShLangRayGen;
  case Stage::Intersect:
    return EShLangIntersect;
  case Stage::AnyHit:
    return EShLangAnyHit;
  case Stage::ClosestHit:
    return EShLangClosestHit;
  case Stage::Miss:
    return EShLangMiss;
  case Stage::Callable:
    return EShLangCallable;
  case Stage::Task:
    return EShLangTask;
  case Stage::Mesh:
    return EShLangMesh;
  }

  std::abort();
}

static std::optional<std::vector<std::uint32_t>>
compileGlsl(const std::filesystem::path &cwd, std::string_view shaderSource,
            shader::glsl::Stage stage) {
  static bool _ = [] {
    glslang::InitializeProcess();
    return false;
  }();
  static_cast<void>(_);

  auto glslangStage = toGlslangStage(stage);

  glslang::TShader shader(glslangStage);
  shader.setEnvInput(glslang::EShSourceGlsl, glslangStage,
                     glslang::EShClientVulkan, 100);
  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);

  auto text = shaderSource.data();
  int textLength = shaderSource.length();
  shader.setStringsWithLengths(&text, &textLength, 1);

  auto msg = static_cast<EShMessages>(EShMsgVulkanRules | EShMsgSpvRules);

  struct Includer final : glslang::TShader::Includer {
    const std::filesystem::path &cwd;
    std::forward_list<std::vector<std::byte>> texts;
    std::forward_list<IncludeResult> results;
    Includer(const std::filesystem::path &cwd) : cwd(cwd) {}

    IncludeResult *includeLocal(const char *headerName,
                                const char *includerName,
                                size_t inclusionDepth) override {
      if (cwd.empty()) {
        return nullptr;
      }

      auto data = readFile(cwd / headerName);
      if (!data) {
        return nullptr;
      }

      auto &text = texts.emplace_front(std::move(*data));

      return &results.emplace_front(
          IncludeResult(headerName, reinterpret_cast<const char *>(text.data()),
                        text.size(), nullptr));
    }

    void releaseInclude(IncludeResult *) override {}
  };

  Includer includer{cwd};
  if (!shader.parse(&g_glslangConfig, 460, EProfile::ECoreProfile, false, true,
                    msg, includer)) {
    std::fprintf(stderr, "%s", shader.getInfoLog());
    std::fprintf(stderr, "%s", shader.getInfoDebugLog());
    return {};
  }

  glslang::SpvOptions options{
      .disableOptimizer = true,
      .compileOnly = stage == shader::glsl::Stage::Library,
  };

  std::vector<std::uint32_t> spirv;
  glslang::GlslangToSpv(*shader.getIntermediate(), spirv, &options);

  if (stage == shader::glsl::Stage::Library) {
    spirv.insert(spirv.begin() + 5,
                 {
                     (2 << 16) | shader::ir::spv::OpCapability,
                     (int)shader::ir::spv::Capability::Linkage,
                 });
  }
  return spirv;
}

std::optional<shader::spv::BinaryLayout>
shader::glsl::parseFile(ir::Context &context, Stage stage,
                        const std::filesystem::path &path) {
  auto optFileContent = readFile(path);
  if (!optFileContent.has_value()) {
    return {};
  }

  auto fileContent = std::move(*optFileContent);
  auto text = std::string_view{
      reinterpret_cast<const char *>(fileContent.data()), fileContent.size()};

  auto spv =
      compileGlsl(std::filesystem::absolute(path).parent_path(), text, stage);

  if (!spv) {
    return {};
  }

  return spv::deserialize(context, *spv,
                          context.getPathLocation(path.string()));
}

std::optional<shader::spv::BinaryLayout>
shader::glsl::parseSource(ir::Context &context, Stage stage,
                          std::string_view source, ir::Location loc) {
  if (loc == nullptr) {
    loc = context.getUnknownLocation();
  }

  auto spv = compileGlsl({}, source, stage);

  if (!spv) {
    return {};
  }

  return spv::deserialize(context, *spv, loc);
}

std::string shader::glsl::decompile(std::span<const std::uint32_t> spv) {
  spvc_context context = nullptr;
  spvc_parsed_ir ir = nullptr;
  spvc_compiler compiler_glsl = nullptr;
  spvc_compiler_options options = nullptr;
  const char *result = nullptr;

  spvc_context_create(&context);

  spvc_context_set_error_callback(
      context,
      [](void *, const char *message) {
        std::fprintf(stderr, "%s\n", message);
      },
      nullptr);

  spvc_context_parse_spirv(context, spv.data(), spv.size(), &ir);
  spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir,
                               SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
                               &compiler_glsl);

  spvc_compiler_create_compiler_options(compiler_glsl, &options);
  spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION,
                                 460);
  spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES,
                                 SPVC_FALSE);
  spvc_compiler_options_set_bool(
      options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_TRUE);
  spvc_compiler_install_compiler_options(compiler_glsl, options);

  if (spvc_compiler_compile(compiler_glsl, &result) != SPVC_SUCCESS) {
    spvc_context_destroy(context);
    return {};
  }
  std::string resultStr = result;
  spvc_context_destroy(context);

  return resultStr;
}
