
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ostream>
#include <rx/Version.hpp>
#include <shader/dialect.hpp>
#include <shader/glsl.hpp>
#include <shader/ir.hpp>
#include <shader/spv.hpp>
#include <string_view>
#include <vector>

#ifdef GCN
#include <shader/GcnConverter.hpp>
#include <shader/gcn.hpp>
#include <shaders/rdna-semantic-spirv.hpp>
#endif

enum class OutputType {
  SpirvBinary,
  SpirvHeader,
  SpirvAssembly,
  Glsl,
  Ir,
};

enum class InputType {
  Glsl,
  SpirvBinary,
  Sb,
  Isa,
};

static std::optional<shader::glsl::Stage>
parseGlslStage(std::string_view stage) {
  if (stage == "library") {
    return shader::glsl::Stage::Library;
  }
  if (stage == "vertex") {
    return shader::glsl::Stage::Vertex;
  }
  if (stage == "tess-control") {
    return shader::glsl::Stage::TessControl;
  }
  if (stage == "tess-evaluation") {
    return shader::glsl::Stage::TessEvaluation;
  }
  if (stage == "geometry") {
    return shader::glsl::Stage::Geometry;
  }
  if (stage == "fragment") {
    return shader::glsl::Stage::Fragment;
  }
  if (stage == "compute") {
    return shader::glsl::Stage::Compute;
  }
  if (stage == "ray-gen") {
    return shader::glsl::Stage::RayGen;
  }
  if (stage == "intersect") {
    return shader::glsl::Stage::Intersect;
  }
  if (stage == "any-hit") {
    return shader::glsl::Stage::AnyHit;
  }
  if (stage == "closest-hit") {
    return shader::glsl::Stage::ClosestHit;
  }
  if (stage == "miss") {
    return shader::glsl::Stage::Miss;
  }
  if (stage == "callable") {
    return shader::glsl::Stage::Callable;
  }
  if (stage == "task") {
    return shader::glsl::Stage::Task;
  }
  if (stage == "mesh") {
    return shader::glsl::Stage::Mesh;
  }

  return {};
}

static std::optional<InputType> parseInputType(std::string_view type) {
  if (type == "glsl") {
    return InputType::Glsl;
  }
  if (type == "spirv-bin") {
    return InputType::SpirvBinary;
  }
  if (type == "sb") {
    return InputType::Sb;
  }
  if (type == "isa") {
    return InputType::Isa;
  }
  return {};
}

static std::optional<OutputType> parseOutputType(std::string_view type) {
  if (type == "glsl") {
    return OutputType::Glsl;
  }
  if (type == "spirv-bin") {
    return OutputType::SpirvBinary;
  }
  if (type == "spirv-header") {
    return OutputType::SpirvHeader;
  }
  if (type == "spirv-asm") {
    return OutputType::SpirvAssembly;
  }
  if (type == "ir") {
    return OutputType::Ir;
  }
  return {};
}

#ifdef GCN
static std::optional<shader::gcn::Stage> parseGcnStage(std::string_view stage) {
  if (stage == "ps") {
    return shader::gcn::Stage::Ps;
  }
  if (stage == "vs-vs") {
    return shader::gcn::Stage::VsVs;
  }
  if (stage == "vs-es") {
    return shader::gcn::Stage::VsEs;
  }
  if (stage == "vs-ls") {
    return shader::gcn::Stage::VsLs;
  }
  if (stage == "cs") {
    return shader::gcn::Stage::Cs;
  }
  if (stage == "gs") {
    return shader::gcn::Stage::Gs;
  }
  if (stage == "gs-vs") {
    return shader::gcn::Stage::GsVs;
  }
  if (stage == "hs") {
    return shader::gcn::Stage::Hs;
  }
  if (stage == "ds-vs") {
    return shader::gcn::Stage::DsVs;
  }
  if (stage == "ds-es") {
    return shader::gcn::Stage::DsEs;
  }

  return {};
}
#endif

struct InputParam {
  std::optional<InputType> type;
  std::optional<shader::glsl::Stage> glslStage;
  bool validate = false;

#ifdef GCN
  std::string semanticPath;
  std::optional<shader::gcn::Stage> gcnStage;
#endif
};

struct OutputParam {
  std::string varName;
  std::optional<OutputType> type;
  bool validate = false;
  int optLevel = 0;
};

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

static void writeSpvHeader(OutputParam &outputParam, std::ostream &out,
                           std::span<const std::uint32_t> spv) {
  out << "#pragma once\n"
      << "#include <cstdint>\n\n"
      << "static const std::uint32_t " << outputParam.varName << "[] = {";

  for (auto word : spv) {
    out << "0x" << std::hex << word << ", ";
  }

  out << "};\n";
}

static bool writeOutput(OutputParam &outputParam, std::ostream &out,
                        shader::ir::Region region) {
  switch (*outputParam.type) {
  case OutputType::SpirvBinary:
  case OutputType::SpirvHeader:
  case OutputType::SpirvAssembly:
  case OutputType::Glsl: {
    auto spv = shader::spv::serialize(region);

    if (outputParam.validate) {
      if (!shader::spv::validate(spv)) {
        return false;
      }
    }

    if (outputParam.optLevel >= 3) {
      if (auto opt = shader::spv::optimize(spv)) {
        spv = *opt;
      }
    }

    if (outputParam.type == OutputType::SpirvBinary) {
      out.write(reinterpret_cast<const char *>(spv.data()),
                spv.size() * sizeof(spv[0]));
    } else if (outputParam.type == OutputType::SpirvHeader) {
      writeSpvHeader(outputParam, out, spv);
    } else if (outputParam.type == OutputType::SpirvAssembly) {
      out << shader::spv::disassembly(spv, true);
    } else if (outputParam.type == OutputType::Glsl) {
      out << shader::glsl::decompile(spv);
    } else {
      return false;
    }

    return true;
  }

  case OutputType::Ir: {
    shader::ir::NameStorage ns;
    region.print(out, ns);
    return true;
  }
  }

  return false;
}

#ifdef GCN
static shader::ir::Region parseIsa(shader::ir::Context &context,
                                   InputParam &inputParam,
                                   OutputParam &outputParam,
                                   shader::ir::Location loc,
                                   std::span<const std::byte> bytes) {
  shader::gcn::Context semanticContext;
  shader::spv::BinaryLayout semanticLayout;

  if (!inputParam.gcnStage) {
    inputParam.gcnStage = shader::gcn::Stage::Cs;
  }

  if (!inputParam.semanticPath.empty()) {
    if (auto result = shader::glsl::parseFile(
            semanticContext, *inputParam.glslStage, inputParam.semanticPath)) {
      semanticLayout = *result;
    } else {
      std::fprintf(stderr, "Failed to parse semantic '%s'\n",
                   inputParam.semanticPath.c_str());
      return {};
    }
  } else {
    if (auto result = shader::spv::deserialize(semanticContext,
                                               g_rdna_semantic_spirv, loc)) {
      semanticLayout = *result;
    } else {
      std::fprintf(stderr, "Failed to parse builtin semantic\n");
      return {};
    }
  }

  shader::gcn::canonicalizeSemantic(semanticContext, semanticLayout);
  shader::gcn::SemanticModuleInfo gcnSemanticModuleInfo;
  shader::gcn::collectSemanticModuleInfo(gcnSemanticModuleInfo, semanticLayout);
  auto gcnSemanticInfo =
      shader::gcn::collectSemanticInfo(gcnSemanticModuleInfo);

  shader::gcn::Context isaContext;
  shader::gcn::Environment env;
  auto ir = shader::gcn::deserialize(
      isaContext, env, gcnSemanticInfo, 0,
      [&](std::uint64_t address) -> std::uint32_t {
        return *reinterpret_cast<const std::uint32_t *>(bytes.data() + address);
      });

  if (outputParam.type == OutputType::Ir) {
    return ir;
  }

  if (auto converted = shader::gcn::convertToSpv(
          isaContext, ir, gcnSemanticModuleInfo, *inputParam.gcnStage, env)) {
    if (auto result = shader::spv::deserialize(context, converted->spv, loc)) {
      return result->merge(context);
    }
  }
  return {};
}

static shader::ir::Region parseSb(shader::ir::Context &context,
                                  InputParam &inputParam,
                                  OutputParam &outputParam,
                                  shader::ir::Location loc,
                                  std::span<std::byte> bytes) {
  auto headerSize = static_cast<std::uint32_t>(bytes[45]) * 4;
  auto instOffset = 52 + headerSize;
  if (!inputParam.gcnStage) {
    inputParam.gcnStage =
        static_cast<shader::gcn::Stage>(unsigned(bytes[8] >> 2) & 0xf);
  }

  return parseIsa(context, inputParam, outputParam, loc,
                  bytes.subspan(instOffset));
}
#endif

static std::optional<shader::ir::Region>
parseFile(shader::ir::Context &context, InputParam &inputParam,
          OutputParam &outputParam, const std::filesystem::path &path) {

  if (!inputParam.type) {
    auto ext = path.extension();
    if (ext == ".glsl") {
      inputParam.type = InputType::Glsl;
    } else if (ext == ".spirv" || ext == ".spv") {
      inputParam.type = InputType::SpirvBinary;
    } else if (ext == ".sb") {
      inputParam.type = InputType::Sb;
    } else {
      return {};
    }
  }

  if (inputParam.type == InputType::Glsl) {
    if (!inputParam.glslStage) {
      auto stageText =
          std::filesystem::path(path).replace_extension().extension();
      if (stageText == ".vert") {
        inputParam.glslStage = shader::glsl::Stage::Vertex;
      } else if (stageText == ".comp") {
        inputParam.glslStage = shader::glsl::Stage::Compute;
      } else if (stageText == ".frag") {
        inputParam.glslStage = shader::glsl::Stage::Fragment;
      } else if (stageText == ".geom") {
        inputParam.glslStage = shader::glsl::Stage::Geometry;
      } else {
        inputParam.glslStage = shader::glsl::Stage::Library;
      }
    }

    if (auto result =
            shader::glsl::parseFile(context, *inputParam.glslStage, path)) {
      return result->merge(context);
    }

    return {};
  }

  if (inputParam.type == InputType::SpirvBinary) {
    auto optFileContent = readFile(path);
    if (!optFileContent.has_value()) {
      return {};
    }
    auto fileContent = std::move(*optFileContent);
    auto data =
        std::span{reinterpret_cast<const std::uint32_t *>(fileContent.data()),
                  fileContent.size() / sizeof(std::uint32_t)};
    auto loc = context.getPathLocation(path.string());
    if (auto result = shader::spv::deserialize(context, data, loc)) {
      return result->merge(context);
    }
  }

#ifdef GCN
  if (inputParam.type == InputType::Sb) {
    auto loc = context.getPathLocation(path.string());
    auto optFileContent = readFile(path);
    if (!optFileContent.has_value()) {
      return {};
    }
    auto fileContent = std::move(*optFileContent);
    return parseSb(context, inputParam, outputParam, loc, fileContent);
  }

  if (inputParam.type == InputType::Isa) {
    auto loc = context.getPathLocation(path.string());
    auto optFileContent = readFile(path);
    if (!optFileContent.has_value()) {
      return {};
    }
    auto fileContent = std::move(*optFileContent);
    return parseIsa(context, inputParam, outputParam, loc, fileContent);
  }
#endif

  return {};
}

void usage(std::FILE *out, const char *argv0) {
  std::fprintf(out, "usage: %s [options] -i <input file> [-o <output file>]\n",
               argv0);
  std::fprintf(out, "\n");
  std::fprintf(out, "  options:\n");

#ifdef GCN
  std::fprintf(out, "    --input-type <glsl|spirv-bin|sb|isa>\n");
  std::fprintf(out, "    --semantic <semantic file>\n");
  std::fprintf(out, "    --input-isa-stage <isa-stage>\n");
#else
  std::fprintf(out, "    --input-type <glsl|spirv-bin>\n");
#endif

  std::fprintf(out, "    --input-glsl-stage <glsl-stage>\n");
  std::fprintf(
      out, "    --output-type <glsl|spirv-bin|spirv-header|spirv-asm|ir>\n");
  std::fprintf(out, "    --validate - validate output spirv\n");
  std::fprintf(out, "    --output-var-name <name> - specify variable name for "
                    "spirv-header\n");
  std::fprintf(out, "    -O<0|1|2|3> - optimize spirv\n");
  std::fprintf(out, "\n");
  std::fprintf(out, "  glsl-stage:\n");
  std::fprintf(out, "    library\n");
  std::fprintf(out, "    vertex\n");
  std::fprintf(out, "    tess-control\n");
  std::fprintf(out, "    tess-evaluation\n");
  std::fprintf(out, "    geometry\n");
  std::fprintf(out, "    fragment\n");
  std::fprintf(out, "    compute\n");
  std::fprintf(out, "    ray-gen\n");
  std::fprintf(out, "    intersect\n");
  std::fprintf(out, "    any-hit\n");
  std::fprintf(out, "    closest-hit\n");
  std::fprintf(out, "    miss\n");
  std::fprintf(out, "    callable\n");
  std::fprintf(out, "    task\n");
  std::fprintf(out, "    mesh\n");
#ifdef GCN
  std::fprintf(out, "\n");
  std::fprintf(out, "  isa-stage:\n");
  std::fprintf(out, "    ps\n");
  std::fprintf(out, "    vs-vs\n");
  std::fprintf(out, "    vs-es\n");
  std::fprintf(out, "    vs-ls\n");
  std::fprintf(out, "    cs\n");
  std::fprintf(out, "    gs\n");
  std::fprintf(out, "    gs-vs\n");
  std::fprintf(out, "    hs\n");
  std::fprintf(out, "    ds-vs\n");
  std::fprintf(out, "    ds-es\n");
#endif
}

int main(int argc, const char *argv[]) {
  const char *inputFile = nullptr;
  const char *outputFile = nullptr;
  InputParam inputParam;
  OutputParam outputParam;

  for (int i = 1; i < argc; ++i) {
    if (argv[i] == std::string_view("-h") ||
        argv[i] == std::string_view("--help")) {
      usage(stdout, argv[0]);
      return 0;
    }
    if (argv[i] == std::string_view("-v") ||
        argv[i] == std::string_view("--version")) {
      std::printf("%s\n", rx::getVersion().toString().c_str());
      return 0;
    }

    if (argv[i] == std::string_view{"--validate"}) {
      outputParam.validate = true;
      continue;
    }

    if (argv[i] == std::string_view{"-O0"}) {
      outputParam.optLevel = 0;
      continue;
    }
    if (argv[i] == std::string_view{"-O1"}) {
      outputParam.optLevel = 1;
      continue;
    }
    if (argv[i] == std::string_view{"-O2"}) {
      outputParam.optLevel = 2;
      continue;
    }
    if (argv[i] == std::string_view{"-O3"}) {
      outputParam.optLevel = 3;
      continue;
    }

    if (i + 1 < argc) {
      const char *key = argv[i];
      const char *value = argv[++i];

      if (key == std::string_view{"-i"} || key == std::string_view{"--input"}) {
        inputFile = value;
        continue;
      }
      if (key == std::string_view{"-o"} ||
          key == std::string_view{"--output"}) {
        outputFile = value;
        continue;
      }

      if (key == std::string_view{"--input-type"}) {
        if (auto inputType = parseInputType(value)) {
          inputParam.type = *inputType;
          continue;
        }
      }

      if (key == std::string_view{"--output-type"}) {
        if (auto outputType = parseOutputType(value)) {
          outputParam.type = *outputType;
          continue;
        }
      }

      if (key == std::string_view{"--input-glsl-stage"}) {
        if (auto glslStage = parseGlslStage(value)) {
          inputParam.glslStage = *glslStage;
          continue;
        }
      }

      if (key == std::string_view{"--output-var-name"}) {
        outputParam.varName = value;
        continue;
      }

#ifdef GCN
      if (key == std::string_view{"--semantic"}) {
        inputParam.semanticPath = value;
        continue;
      }

      if (key == std::string_view{"--input-isa-stage"}) {
        if (auto stage = parseGcnStage(value)) {
          inputParam.gcnStage = *stage;
          continue;
        }
      }
#endif
    }

    usage(stderr, argv[0]);
    return 1;
  }

  if (outputFile == nullptr) {
    outputFile = "-";
  }

  if (inputFile == nullptr) {
    usage(stderr, argv[0]);
    return 1;
  }

  if (!outputParam.type) {
    outputParam.type = OutputType::Ir;
  }

  if (outputParam.varName.empty()) {
    outputParam.varName = std::filesystem::path(inputFile)
                              .filename()
                              .replace_extension()
                              .string();
    for (auto &c : outputParam.varName) {
      if (c == '.') {
        c = '_';
      }
    }
  }

  shader::ir::Context context;
  auto ir = parseFile(context, inputParam, outputParam, inputFile);
  if (!ir) {
    std::fprintf(stderr, "failed to parse '%s'\n", inputFile);
    return 1;
  }

  std::ofstream outputFileStream;

  if (outputFile != std::string_view("-")) {
    outputFileStream = std::ofstream(outputFile, std::ios::binary);
  }

  std::ostream &ostream =
      (outputFile == std::string_view("-") ? std::cout : outputFileStream);

  if (!ostream) {
    std::fprintf(stderr, "failed to create '%s'\n", outputFile);
    return 1;
  }

  if (!writeOutput(outputParam, ostream, *ir)) {
    return 1;
  }

  if (!ostream) {
    std::fprintf(stderr, "failed to write to '%s'\n", outputFile);
    return 1;
  }

  return 0;
}
