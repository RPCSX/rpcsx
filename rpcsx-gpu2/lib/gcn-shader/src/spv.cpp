#include "spv.hpp"
#include "dialect.hpp"
#include "ir/Kind.hpp"
#include <iostream>

using namespace shader;

static std::uint32_t generateSpv(std::vector<std::uint32_t> &result,
                                 shader::ir::Region body) {
  std::map<shader::ir::Value, std::uint32_t> valueToId;
  std::uint32_t bounds = 1;

  auto getValueId = [&](shader::ir::Value value) {
    auto [it, inserted] = valueToId.emplace(value, 0);
    if (inserted) {
      it->second = bounds++;
    }
    return it->second;
  };

  for (auto child : body.children()) {
    auto instruction = child.cast<shader::ir::Instruction>();
    if (instruction == nullptr) {
      std::fprintf(stderr, "generate spv: unexpected node\n");
      std::abort();
    }
    if (instruction.getKind() != shader::ir::Kind::Spv) {
      std::fprintf(
          stderr, "generate spv: unexpected instruction: %s\n",
          ir::getInstructionName(instruction.getKind(), instruction.getOp())
              .c_str());
      std::abort();
    }

    std::size_t headerWordIndex = result.size();
    result.emplace_back() = instruction.getOp();

    auto addWord = [&](std::uint32_t word) { result.emplace_back() = word; };
    auto addDWord = [&](std::uint64_t dword) {
      addWord(dword);
      addWord(dword >> 32);
    };

    auto addString = [&](std::string_view string) {
      auto stringOffset = result.size();
      result.resize(result.size() + string.size() / sizeof(std::uint32_t) + 1);
      std::memcpy(result.data() + stringOffset, string.data(), string.size());
    };

    auto operands = child.getOperands();

    if (auto value = instruction.cast<ir::Value>()) {
      if (!ir::spv::isTypeOp(value.getOp())) {
        if (!operands.empty()) {
          if (auto typeOperand = operands[0].getAsValue()) {
            addWord(getValueId(typeOperand));
            operands = operands.subspan(1);
          }
        }
      }

      addWord(getValueId(value));
    }

    for (auto operand : operands) {
      if (auto value = operand.getAsValue()) {
        addWord(getValueId(value));
        continue;
      }

      if (auto value = operand.getAsString()) {
        addString(*value);
        continue;
      }

      if (auto value = operand.getAsInt32()) {
        addWord(*value);
        continue;
      }

      if (auto value = operand.getAsBool()) {
        addWord(*value ? 1 : 0);
        continue;
      }

      if (auto value = operand.getAsFloat()) {
        addWord(std::bit_cast<std::uint32_t>(*value));
        continue;
      }

      if (auto value = operand.getAsInt64()) {
        addDWord(*value);
        continue;
      }

      if (auto value = operand.getAsDouble()) {
        addDWord(std::bit_cast<std::uint64_t>(*value));
        continue;
      }

      std::fprintf(stderr, "unsupported operand\n");
      shader::ir::NameStorage ns;
      operand.print(std::cerr, ns);
      std::abort();
    }

    result[headerWordIndex] |= (result.size() - headerWordIndex) << 16;
  }

  return bounds;
}

std::optional<shader::spv::BinaryLayout>
shader::spv::deserialize(ir::Context &context,
                         std::span<const std::uint32_t> spv, ir::Location loc) {
  if (loc == nullptr) {
    loc = context.getUnknownLocation();
  }

  shader::spv::BinaryLayout layout;
  if (shader::ir::spv::deserialize(context, loc, layout, spv.subspan(5))) {
    return layout;
  }

  return {};
}

std::vector<std::uint32_t> shader::spv::serialize(ir::Region body) {
  std::vector<std::uint32_t> result;
  result.resize(5);
  result[0] = 0x07230203;
  result[1] = 0x00010400;
  result[3] = generateSpv(result, body);
  return result;
}

bool spv::isTerminatorInst(ir::InstructionId inst) {
  return inst == ir::spv::OpReturn || inst == ir::spv::OpReturnValue ||
         inst == ir::spv::OpKill || inst == ir::spv::OpTerminateInvocation ||
         inst == ir::spv::OpBranch || inst == ir::spv::OpBranchConditional ||
         inst == ir::spv::OpSwitch || inst == ir::spv::OpUnreachable;
}

void shader::spv::dump(std::span<const std::uint32_t> spv, bool pretty) {
  std::cerr << disassembly(spv, pretty);
}

std::string shader::spv::disassembly(std::span<const std::uint32_t> spv,
                                     bool pretty) {
  spv_target_env target_env = SPV_ENV_VULKAN_1_2;
  spv_context spvContext = spvContextCreate(target_env);
  spv_diagnostic diagnostic = nullptr;

  int options = SPV_BINARY_TO_TEXT_OPTION_COMMENT |
                SPV_BINARY_TO_TEXT_OPTION_INDENT;
  if (pretty) {
    options |= SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
  }

  spv_text text{};

  spv_result_t error = spvBinaryToText(spvContext, spv.data(), spv.size(),
                                       options, &text, &diagnostic);

  if (error != 0) {
    spvDiagnosticPrint(diagnostic);
  }

  std::string result;
  if (text != nullptr) {
    result = std::string(text->str, text->length);
  }

  spvDiagnosticDestroy(diagnostic);
  spvContextDestroy(spvContext);
  return result;
}

bool shader::spv::validate(std::span<const std::uint32_t> spv) {
  spv_target_env target_env = SPV_ENV_VULKAN_1_3;
  spv_context spvContext = spvContextCreate(target_env);
  spv_diagnostic diagnostic = nullptr;

  spv_const_binary_t cBin{
      .code = spv.data(),
      .wordCount = spv.size(),
  };

  auto options = spvValidatorOptionsCreate();
  spvValidatorOptionsSetScalarBlockLayout(options, true);

  bool success = spvValidateWithOptions(spvContext, options, &cBin,
                                        &diagnostic) == SPV_SUCCESS;
  if (!success) {
    spvDiagnosticPrint(diagnostic);
  }

  spvValidatorOptionsDestroy(options);
  spvDiagnosticDestroy(diagnostic);
  spvContextDestroy(spvContext);

  return success;
}

std::optional<std::vector<uint32_t>>
shader::spv::optimize(std::span<const std::uint32_t> spv) {
  spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_2);
  for (int i = 0; i < 100; ++i) {
    optimizer.RegisterPerformancePasses();
    optimizer.RegisterSizePasses();
  }

  std::vector<uint32_t> result;
  result.reserve(spv.size());

  spvtools::ValidatorOptions options;
  options.SetSkipBlockLayout(true);

  if (!optimizer.Run(spv.data(), spv.size(), &result, options, true)) {
    return {};
  }

  return result;
}
