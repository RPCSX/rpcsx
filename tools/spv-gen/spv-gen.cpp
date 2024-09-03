#include <bit>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

using json = nlohmann::json;

inline std::string unwrapType(const std::string &type) {
  if (type == "LiteralFloat")
    return "float";
  if (type == "LiteralString")
    return "std::string";
  if (type == "LiteralInteger")
    return "std::int32_t";
  if (type == "LiteralExtInstInteger")
    return "std::int32_t";
  if (type == "LiteralSpecConstantOpInteger")
    return "std::int32_t";
  return type;
}

enum class InstructionKind {
  Value,
  Type,
  Instruction,
};

static InstructionKind getInstructionKind(const json &inst) {
  if (inst.contains("class") && inst["class"] == "Type-Declaration") {
    return InstructionKind::Type;
  }
  if (inst["opname"].get<std::string>().starts_with("OpType")) {
    return InstructionKind::Type;
  }

  if (!inst.contains("operands")) {
    return InstructionKind::Instruction;
  }

  bool hasResult = false;
  bool hasResultType = false;
  for (auto &operand : inst["operands"]) {
    auto kind = operand["kind"].get<std::string>();

    if (hasResult == false && kind == "IdResult") {
      hasResult = true;
      continue;
    }

    if (hasResultType == false && kind == "IdResultType") {
      hasResultType = true;
      continue;
    }
  }

  if (hasResult) {
    return InstructionKind::Value;
  }

  return InstructionKind::Instruction;
}

struct EnumField {
  std::uint32_t value;
  std::string name;
  std::vector<std::string> params;
};

struct EnumDefinition {
  bool isBitEnum;
  std::vector<EnumField> fields;
};

inline void generateInstructions(std::set<std::string> &composites,
                                 std::map<std::string, EnumDefinition> &enums,
                                 const json &json, std::string instKind) {
  std::string instructionNameBody;

  std::printf("enum Op {\n");
  for (auto inst : json["instructions"]) {
    auto opcode = inst["opcode"].get<unsigned>();
    auto name = inst["opname"].get<std::string>();

    std::printf("  %s = %u,\n", name.c_str(), opcode);
  }
  std::printf("};\n");

  std::printf("template<typename ImplT>\n");
  std::printf("struct Builder : BuilderFacade<Builder<ImplT>, ImplT> {\n");

  std::string instructionDecoderBody;
  std::set<std::uint32_t> typeCreateOps;

  std::unordered_set<unsigned> inserted;
  for (auto &inst : json["instructions"]) {
    auto name = inst["opname"].get<std::string>();
    auto opcode = inst["opcode"].get<unsigned>();
    auto kind = instKind == "Glsl450" ? InstructionKind::Value
                                      : getInstructionKind(inst);
    if (name.starts_with("Op")) {
      name = name.substr(2);
    }

    if (kind == InstructionKind::Type) {
      typeCreateOps.insert(opcode);
    }

    switch (kind) {
    case InstructionKind::Value:
    case InstructionKind::Type:
      std::printf("  Value ");
      break;
      // std::printf("  Type ");
      // break;
    case InstructionKind::Instruction:
      std::printf("  Instruction ");
      break;
    }

    std::printf("create%s%s(Location location", instKind.c_str(), name.c_str());

    bool hasTypeArg = false;

    auto operands = inst.contains("operands") ? json::array_t(inst["operands"])
                                              : json::array_t{};

    for (std::size_t index = 0; auto operand : operands) {
      auto argType = operand["kind"].get<std::string>();

      if (argType == "IdResult") {
        continue;
      }

      if (argType == "IdResultType") {
        hasTypeArg = true;
        std::printf(", Value type");
        continue;
      }

      std::printf(", ");

      // if (argType == "IdRef" && operand.contains("name") &&
      //     operand["name"].get<std::string>().contains("ype")) {
      //   argType = "Type";
      // }

      argType = unwrapType(argType);

      char quantifier = ' ';
      if (operand.contains("quantifier")) {
        quantifier = operand["quantifier"].get<std::string>()[0];
      }

      if (quantifier == '*') {
        argType = "std::span<const " + argType + ">";
      } else if (quantifier == '?') {
        argType = "std::optional<" + argType + ">";
      } else if (quantifier != ' ') {
        std::abort();
      }

      std::printf("%s %s",
                  argType == "LiteralContextDependentNumber" ? "auto"
                                                             : argType.c_str(),
                  ("arg" + std::to_string(index++)).c_str());

      if (quantifier != ' ') {
        std::printf(" = {}");
      }
    }

    std::printf(") {\n");

    if (inserted.insert(opcode).second) {
      instructionNameBody +=
          "    case " + std::to_string(opcode) + ": return \"" + name + "\";\n";

      instructionDecoderBody += "    case " + std::to_string(opcode) + ": {\n";

      if (inst["opname"] != "OpVariable") {
        instructionDecoderBody +=
            "      auto builder = "
            "ir::Builder<Builder>::createAppend(context, layout.getOrCreate";
        std::string instClass = "Functions";
        if (inst.contains("class")) {
          if (inst["class"] == "Debug" || inst["class"] == "Annotation") {
            instClass = inst["class"];
            instClass += 's';
          } else if (inst["class"] == "Mode-Setting") {
            if (inst["opname"] == "OpExecutionMode") {
              instClass = "ExecutionModes";
            } else if (inst["opname"] == "OpCapability") {
              instClass = "Capabilities";
            } else if (inst["opname"] == "OpMemoryModel") {
              instClass = "MemoryModels";
            } else if (inst["opname"] == "OpEntryPoint") {
              instClass = "EntryPoints";
            }
          } else if (inst["class"] == "Type-Declaration" ||
                     inst["class"] == "Constant-Creation") {
            instClass = "Globals";
          } else if (inst["class"] == "Extension") {
            if (inst["opname"] == "OpExtInstImport") {
              instClass = "ExtInstImports";
            } else if (inst["opname"] == "OpExtension") {
              instClass = "Extensions";
            }
          }
        }
        instructionDecoderBody += instClass;
        instructionDecoderBody += "(context));\n";
      }

      if (!operands.empty()) {
        std::vector<std::string> args;
        std::vector<std::string> argNames;
        std::string additionalOperands;
        bool hasResultIdArg = false;

        for (std::size_t index = 0; auto &operand : operands) {
          auto argType = operand["kind"].get<std::string>();
          if (argType == "IdResult") {
            auto &arg = args.emplace_back();
            arg += "auto id = instWords[wordIndex++];";
            hasResultIdArg = true;
            continue;
          }

          if (hasTypeArg && argType == "IdResultType") {
            auto &arg = args.emplace_back();
            arg += "auto typeValue = findValue(instWords[wordIndex++]);";
            additionalOperands += "      operands.addOperand(typeValue);\n";
            continue;
          }

          auto argName = "arg" + std::to_string(index++);

          char quantifier = ' ';
          if (operand.contains("quantifier")) {
            quantifier = operand["quantifier"].get<std::string>()[0];
          }

          std::string unwrapOp;
          std::string unwrapSecondOp;

          argType = unwrapType(argType);

          if (enums.contains(argType)) {
            if (quantifier == '*') {
              additionalOperands +=
                  "      while (wordIndex < instWords.size()) {\n";
              additionalOperands += "  ";
            } else if (quantifier == '?') {
              additionalOperands +=
                  "      if (wordIndex < instWords.size()) {\n";
              additionalOperands += "  ";
            }
            additionalOperands += "      deserialize";
            additionalOperands += argType;
            additionalOperands += "(operands, instWords);\n";

            if (quantifier != ' ') {
              additionalOperands += "      }\n";
            }

            continue;
          }

          bool isPair = false;

          if (argType.starts_with("Id")) {
            unwrapOp = "findValue";
          } else if (argType == "float") {
            unwrapOp = "reinterpret_cast<float>";
          } else if (argType == "std::string") {
            unwrapOp = "readString";
          } else if (argType != "std::int32_t") {
            if (argType.starts_with("Pair")) {
              if (argType == "PairIdRefLiteralInteger") {
                unwrapOp = "findValue";
              } else if (argType == "PairIdRefIdRef") {
                unwrapOp = "findValue";
                unwrapSecondOp = "findValue";
              } else if (argType == "PairLiteralIntegerIdRef") {
                unwrapSecondOp = "findValue";
              } else {
                std::fprintf(stderr, "%s\n", argType.c_str());
                std::abort();
              }

              isPair = true;
            } else if (argType == "LiteralContextDependentNumber") {
              if (opcode == 43 || opcode == 50) {
                // OpConstant / OpSpecConstant

                additionalOperands += R"c++(
      if (typeValue.getOp() == OpTypeFloat) {
        auto width = typeValue.getOperand(0).getAsInt32();
        if (width == nullptr) {
          return false;
        }

        if (*width == 32) {
          operands.addOperand(std::bit_cast<float>(instWords[wordIndex++]));
        } else if (*width == 64) {
          auto lo = instWords[wordIndex++];
          auto hi = instWords[wordIndex++];
          operands.addOperand(std::bit_cast<double>((static_cast<std::uint64_t>(hi) << 32) | lo));
        } else {
          return false;
        }
      } else if (typeValue.getOp() == OpTypeInt) {
        auto width = typeValue.getOperand(0).getAsInt32();
        if (width == nullptr) {
          return false;
        }

        if (*width <= 32) {
          operands.addOperand(instWords[wordIndex++]);
        } else if (*width == 64) {
          auto lo = instWords[wordIndex++];
          auto hi = instWords[wordIndex++];
          operands.addOperand((static_cast<std::uint64_t>(hi) << 32) | lo);
        } else {
          return false;
        }
      } else {
        return false;
      }
)c++";
                continue;
              }
              unwrapOp = "static_cast<" + argType + ">";
            } else {
              unwrapOp = "static_cast<" + argType + ">";
            }
          }

          if (quantifier == '*') {
            if (isPair) {
              additionalOperands +=
                  "      while (wordIndex + 1 < instWords.size()) {\n";
              additionalOperands += "        operands.addOperand(";
              if (!unwrapOp.empty()) {
                additionalOperands += unwrapOp;
                additionalOperands += "(instWords[wordIndex++])";
              } else {
                additionalOperands += "instWords[wordIndex++]";
              }
              additionalOperands += ");\n";

              additionalOperands += "        operands.addOperand(";
              if (!unwrapSecondOp.empty()) {
                additionalOperands += unwrapSecondOp;
                additionalOperands += "(instWords[wordIndex++])";
              } else {
                additionalOperands += "instWords[wordIndex++]";
              }
              additionalOperands += ");\n";
              additionalOperands += "      }\n";
            } else {
              additionalOperands +=
                  "      while (wordIndex < instWords.size()) {\n";
              additionalOperands += "        operands.addOperand(";
              if (!unwrapOp.empty()) {
                additionalOperands += unwrapOp;
                additionalOperands += "(instWords[wordIndex++])";
              } else {
                additionalOperands += "instWords[wordIndex++]";
              }
              additionalOperands += ");\n";
              additionalOperands += "      }\n";
            }
            continue;
          }

          if (quantifier == '?') {
            additionalOperands += "      if (instWords.size() > wordIndex) {\n";
            additionalOperands += "        operands.addOperand(";
            if (!unwrapOp.empty()) {
              additionalOperands += unwrapOp;
              additionalOperands += "(instWords[wordIndex++])";
            } else {
              additionalOperands += "instWords[wordIndex++]";
            }
            additionalOperands += ");\n";
            additionalOperands += "      }\n";
            continue;
          }

          if (quantifier != ' ') {
            std::abort();
          }

          auto &arg = args.emplace_back();
          arg = "auto ";
          arg += argName;
          arg += "  = ";

          if (!unwrapOp.empty()) {
            arg += unwrapOp;
            arg += "(instWords[wordIndex++]);";
          } else {
            arg += "instWords[wordIndex++];";
          }

          additionalOperands += "      operands.addOperand(";
          additionalOperands += argName;
          additionalOperands += ");\n";
        }

        for (const auto &arg : args) {
          instructionDecoderBody += "      ";
          instructionDecoderBody += arg;
          instructionDecoderBody += "\n";
        }

        instructionDecoderBody += "      OperandList operands;\n";
        instructionDecoderBody += additionalOperands;

        if (inst["opname"] == "OpVariable") {
          instructionDecoderBody +=
              "      auto builder = "
              "ir::Builder<Builder>::createAppend(context, arg0 != "
              "StorageClass::Function ? layout.getOrCreateGlobals(context) : "
              "layout.getOrCreateFunctions(context));\n";
        }

        if (hasResultIdArg) {
          if (hasTypeArg) {
            instructionDecoderBody +=
                "      auto inst = builder.template create<Value>(loc, ";
          } else {
            if (kind == InstructionKind::Type) {
              instructionDecoderBody +=
                  "      auto inst = builder.template create<Value>(loc, ";
            } else {
              instructionDecoderBody +=
                  "      auto inst = builder.template create<Value>(loc, ";
            }
          }
        } else {
          instructionDecoderBody +=
              "      auto inst = builder.template create<Instruction>(loc, ";
        }

        instructionDecoderBody += "Kind::";
        instructionDecoderBody += instKind;
        instructionDecoderBody += ", ";
        instructionDecoderBody += std::to_string(opcode);
        instructionDecoderBody += ", std::move(operands));\n";

        if (hasResultIdArg) {
          instructionDecoderBody += "      addValue(id, inst);\n";
        }
      } else {
        instructionDecoderBody +=
            "      builder.template create<Instruction>(loc, Kind::";
        instructionDecoderBody += instKind;
        instructionDecoderBody += ", ";
        instructionDecoderBody += std::to_string(opcode);
        instructionDecoderBody += ");\n";
      }
      instructionDecoderBody += "      break;\n";
      instructionDecoderBody += "    }\n";
    }

    std::string typeName = name;
    if (typeName.starts_with("Type")) {
      typeName = typeName.substr(std::strlen("Type"));
    }

    std::printf("    OperandList operands;\n");
    if (hasTypeArg) {
      std::printf("    operands.addOperand(type);\n");
    }

    for (std::size_t index = 0; auto operand : operands) {
      auto argType = operand["kind"].get<std::string>();
      if (argType == "IdResult") {
        continue;
      }
      if (argType == "IdResultType") {
        continue;
      }

      char quantifier = ' ';
      if (operand.contains("quantifier")) {
        quantifier = operand["quantifier"].get<std::string>()[0];
      }

      auto argName = "arg" + std::to_string(index++);

      if (quantifier == '*') {
        std::printf("    for (auto arg : %s) {\n  ", argName.c_str());
        argName = "arg";
      } else if (quantifier == '?') {
        std::printf("    if (%s) {\n  ", argName.c_str());
        argName = "*" + argName;
      } else if (quantifier != ' ') {
        std::abort();
      }

      if (composites.contains(operand["kind"])) {
        std::printf("    std::apply([&](auto... args) { "
                    "(operands.addOperand(args), ...); }, %s);\n",
                    argName.c_str());
      } else {
        if (!enums.contains(argType)) {
          std::printf("    operands.addOperand(%s);\n", argName.c_str());
        } else {
          if (argName.starts_with('*')) {
            std::printf("    %s->forwardOperands(operands);\n",
                        argName.substr(1).c_str());
          } else {
            std::printf("    %s.forwardOperands(operands);\n", argName.c_str());
          }
        }
      }
      // if (quantifier == '?') {
      //   std::printf("    } else {\n");
      //   std::printf("      result.addOperand(nullptr);\n");
      // }
      if (quantifier == '?' || quantifier == '*') {
        std::printf("    }\n");
      }
    }

    switch (kind) {
    case InstructionKind::Value:
      std::printf("    return this->template create<Value>(location, ");
      std::printf("Kind::%s, %u, std::move(operands));\n", instKind.c_str(),
                  inst["opcode"].get<unsigned>());
      break;
    case InstructionKind::Type:
      std::printf("    return this->template create<Value>(location, ");
      std::printf("Kind::%s, %u, std::move(operands));\n", instKind.c_str(),
                  inst["opcode"].get<unsigned>());
      break;

    case InstructionKind::Instruction:
      std::printf(
          "    return this->template "
          "create<Instruction>(location, Kind::%s, %u, std::move(operands));\n",
          instKind.c_str(), inst["opcode"].get<unsigned>());
      break;
    }

    // std::printf("    return result;\n");

    std::printf("  }\n");
  }

  std::printf("};\n");

  std::printf("inline const char *getInstructionName(unsigned op) {\n");
  std::printf("  switch (op) {\n");
  std::printf("%s", instructionNameBody.c_str());
  std::printf("  }\n");
  std::printf("  return nullptr;\n");
  std::printf("}\n");

  instructionNameBody = {};

  if (instKind != "Spv") {
    return;
  }

  std::printf(
      R"c++(
inline bool deserialize(Context &context, Location loc, auto &layout, std::span<const std::uint32_t> words) {
  std::unordered_map<std::uint32_t, ir::Value> values;
  std::size_t wordIndex = 0;

  auto readString = [&](const std::uint32_t &word) {
    auto result = reinterpret_cast<const char *>(&word);
    wordIndex += std::strlen(result) / 4;
    return result;
  };

  auto addValue = [&](std::uint32_t id, ir::Value value) {
    auto &prev = values[id];
    if (prev) {
      prev.replaceAllUsesWith(value);
      prev.erase();
    }
    prev = value;
    return value;
  };

  auto findValue = [&](std::uint32_t id) {
    auto [it, inserted] = values.emplace(id, nullptr);
    if (inserted) {
      it->second = ir::Builder<Builder>::createAppend(context, layout.getOrCreateFunctions(context)).createSpvUndef(loc, nullptr);
    }
    return it->second;
  };
)c++");

  for (auto &[name, enumDef] : enums) {
    std::printf(
        "  auto deserialize%s = [&](OperandList &operands, std::span<const "
        "std::uint32_t> instWords) {\n",
        name.c_str());
    std::printf("    auto mask = instWords[wordIndex++];\n");
    std::printf("    operands.addOperand(mask);\n");

    if (!enumDef.isBitEnum) {
      std::printf("    switch (mask) {\n");
    }

    std::unordered_set<std::uint32_t> inserted;

    for (auto &field : enumDef.fields) {
      if (field.params.empty()) {
        continue;
      }

      if (!inserted.emplace(field.value).second) {
        continue;
      }

      if (enumDef.isBitEnum) {
        std::printf("    if (mask & %#x) {\n", field.value);
        for (auto &param : field.params) {
          std::printf("      operands.addOperand(");
          if (param == "float") {
            std::printf("std::bit_cast<float>(instWords[wordIndex++])");
          } else if (param == "std::string") {
            std::printf("readString(instWords[wordIndex++])");
          } else {
            std::printf("instWords[wordIndex++]");
          }

          std::printf(");\n");
        }
        std::printf("    }\n");
      } else {
        std::printf("    case %d:\n", field.value);
        for (auto &param : field.params) {
          std::printf("      operands.addOperand(");
          if (param == "float") {
            std::printf("std::bit_cast<float>(instWords[wordIndex++])");
          } else if (param == "std::string") {
            std::printf("readString(instWords[wordIndex++])");
          } else {
            std::printf("instWords[wordIndex++]");
          }

          std::printf(");\n");
        }
        std::printf("      break;\n");
      }
    }
    if (!enumDef.isBitEnum) {
      std::printf("    }\n");
    }
    std::printf("  };\n");
  }

  std::printf(R"c++(
  auto deserializeInstruction = [&](std::uint32_t op, std::span<const std::uint32_t> instWords) {
    switch (op) {
%s

    default:
      return false;
    }
    return true;
  };

  while (!words.empty()) {
    auto op = words[0] & 0xffff;
    auto wordCount = words[0] >> 16;

    if (wordCount == 0) {
      std::abort();
    }

    auto instWords = words.subspan(1, wordCount - 1);
    words = words.subspan(wordCount);

    wordIndex = 0;
    if (!deserializeInstruction(op, instWords)) {
      return false;
    }

    if (instWords.size() != wordIndex) {
      std::abort();
    }
  }

  return true;
}
)c++",
              instructionDecoderBody.c_str());

  std::printf("inline bool isTypeOp(std::uint32_t op) {\n");
  std::printf("  switch (op) {\n");
  for (auto op : typeCreateOps) {
    std::printf("  case %u:\n", op);
  }
  std::printf("    return true;\n");
  std::printf("  default:\n");
  std::printf("    return false;\n");
  std::printf("  }\n");
  std::printf("}\n");
}

inline void generateGrammar(const json &coreJson, const json &glsl450) {
  std::printf("#pragma once\n");
  std::printf("#include <tuple>\n");
  std::printf("#include <optional>\n");
  std::printf("#include <span>\n");
  std::printf("#include <cstring>\n");
  std::printf("#include <unordered_map>\n");
  std::printf("#include <map>\n");
  std::printf("#include <shader/ir.hpp>\n");
  std::printf("\n");
  std::printf("namespace shader::ir::spv {\n");
  std::set<std::string> composites;
  std::printf("using IdImpl = ValueImpl;\n");
  std::printf("using Id = ValueWrapper<IdImpl>;\n");
  std::printf("using Literal = Operand;\n");

  std::map<std::string, EnumDefinition> enums;
  std::set<std::string> simpleEnums;

  for (const auto &opKind : coreJson["operand_kinds"]) {
    const auto &category = opKind["category"];

    if (category == "Id") {
      auto kind = opKind["kind"].get<std::string>();

      std::printf("using %sImpl = IdImpl;\n", kind.c_str());
      std::printf("using %s = ValueWrapper<%sImpl>;\n", kind.c_str(),
                  kind.c_str());
      continue;
    }

    bool isBitEnum = category == "BitEnum";
    if (isBitEnum || category == "ValueEnum") {
      bool isSimple = true;
      for (const auto &enumerant : opKind["enumerants"]) {
        if (enumerant.contains("parameters")) {
          isSimple = false;
          break;
        }
      }

      if (isSimple) {
        simpleEnums.insert(opKind["kind"]);
        continue;
      }

      std::vector<EnumField> fields;
      for (const auto &enumerant : opKind["enumerants"]) {
        auto name = enumerant["enumerant"].get<std::string>();
        auto value = enumerant["value"];
        auto valueInt = value.is_string()
                            ? std::stoul(value.get<std::string>(), nullptr, 0)
                            : value.get<unsigned>();

        if (isBitEnum && valueInt == 0) {
          continue;
        }

        auto &field = fields.emplace_back();
        if (name[0] >= '0' && name[0] <= '9') {
          field.name = '_';
          field.name += name;
        } else {
          field.name = std::move(name);
        }

        field.value = valueInt;

        if (enumerant.contains("parameters")) {
          for (const auto &param : enumerant["parameters"]) {
            field.params.emplace_back(unwrapType(param["kind"]));
          }
        }
      }

      enums[opKind["kind"]] = {
          .isBitEnum = isBitEnum,
          .fields = std::move(fields),
      };
    }
  }

  for (const auto &opKind : coreJson["operand_kinds"]) {
    if (opKind["category"] == "Composite") {
      auto kind = opKind["kind"].get<std::string>();

      std::printf("using %s = std::tuple<", kind.c_str());
      composites.insert(kind);

      for (bool first = true; auto base : opKind["bases"]) {
        if (first) {
          first = false;
        } else {
          std::printf(", ");
        }
        auto baseString = unwrapType(base);
        std::printf("%s", baseString.c_str());
      }
      std::printf(">;\n");
    }
  }

  for (const auto &opKind : coreJson["operand_kinds"]) {
    const auto &category = opKind["category"];

    if (category == "Literal") {
      //   auto name = kind.get<std::string>();
      //   std::printf("struct %sImpl : LiteralImpl {\n", name.c_str());
      //   std::printf("  using NativeStorageType = NativeStorageFor<%s>;\n",
      //               name.c_str());
      //   std::printf("  NativeStorageType value;\n");
      //   std::printf("  %sImpl() = default;\n", name.c_str());
      //   std::printf("  %sImpl(NativeStorageType value) : value(value)
      //   {}\n",
      //               name.c_str());
      //   std::printf("};\n");
      //   std::printf("struct %s : NodeWrapper<%sImpl> { using "
      //               "NodeWrapper::NodeWrapper; };\n",
      //               name.c_str(), name.c_str());
      continue;
    }

    auto kind = opKind["kind"].get<std::string>();

    if (category == "Composite" || category == "Id") {
      continue;
    }

    bool isBitEnum = category == "BitEnum";
    if (!isBitEnum && category != "ValueEnum") {
      std::fprintf(stderr, "unknown operand category %s\n",
                   category.get<std::string>().c_str());
      std::abort();
    }

    if (simpleEnums.contains(kind)) {
      std::printf("enum class %s {\n", kind.c_str());

      for (auto enumerant : opKind["enumerants"]) {
        auto name = enumerant["enumerant"].get<std::string>();
        auto value = enumerant["value"];
        if (name[0] >= '0' && name[0] <= '9') {
          name = "_" + name;
        }

        if (value.is_string()) {
          std::printf("  %s = %s,\n", name.c_str(),
                      value.get<std::string>().c_str());
        } else {
          std::printf("  %s = %u,\n", name.c_str(), value.get<unsigned>());
        }
      }

      std::printf("};\n");
      continue;
    }
  }

  for (auto &[kind, enumDef] : enums) {
    std::printf("struct %s {\n", kind.c_str());

    for (auto &field : enumDef.fields) {
      std::printf("  struct %s;\n", field.name.c_str());
    }

    if (enumDef.isBitEnum) {
      std::printf(
          R"c++(
  struct None;

  void forwardOperands(std::vector<Operand> &to) {
    unsigned mask = 0;
    for (auto &[id, operands] : fields) {
      mask |= static_cast<std::uint32_t>(1) << id;
    }

    to.push_back(mask);

    for (auto &[id, operands] : fields) {
      for (auto operand : operands) {
        to.push_back(std::move(operand));
      }
    }
  }

  void forwardOperands(ir::Instruction to) {
    forwardOperands(to.get()->operands);
  }

  using Self = %s;
  Self &&operator|(Self &&other) && {
    *this |= std::move(other);
    return std::move(*this);
  }
  Self &&operator|(const Self &other) && {
    return std::move(*this) | Self(other);
  }

  Self operator|(Self &&other) const & {
    return Self(*this) | std::move(other);
  }

  Self operator|(const Self &other) const & {
    return Self(*this) | Self(other);
  }

  Self &operator|=(const Self &other) {
    *this |= Self(other);
    return *this;
  }

  Self &operator|=(Self &&other) {
    for (auto &[id, operands] : other.fields) {
      fields[id] = std::move(operands);
    }

    return *this;
  }

protected:
  std::map<unsigned, std::vector<ir::Operand>> fields;
)c++",
          kind.c_str());
    } else {
      std::printf(
          R"c++(
  void forwardOperands(ir::Instruction to) {
    forwardOperands(to.get()->operands);
  }

  void forwardOperands(std::vector<Operand> &to) {
    to.push_back(id);

    for (auto operand : operands) {
      to.push_back(std::move(operand));
    }
  }

protected:
  std::uint32_t id = 0;
  std::vector<Operand> operands;
)c++");
    }

    std::printf("\n");
    std::printf("  %s() = default;\n", kind.c_str());
    std::printf("};\n");
    std::printf("\n");
  }

  for (auto &[kind, enumDef] : enums) {
    auto isBitEnum = enumDef.isBitEnum;
    auto &fields = enumDef.fields;
    if (isBitEnum) {
      std::printf("struct %s::None : %s{};\n", kind.c_str(), kind.c_str());
    }

    for (auto &field : fields) {
      std::printf("struct %s::%s : %s {\n", kind.c_str(), field.name.c_str(),
                  kind.c_str());
      if (isBitEnum) {
        std::printf("  static constexpr std::uint32_t Index = %u;\n",
                    std::countr_zero(field.value));
        std::printf("  static constexpr std::uint32_t Id = std::uint32_t(1) << "
                    "Index;\n");
      } else {
        std::printf("  static constexpr std::uint32_t Id = %u;\n", field.value);
      }

      std::printf("\n");
      std::printf("  %s(", field.name.c_str());
      for (std::size_t index = 0; auto &param : field.params) {
        if (index != 0) {
          std::printf(", ");
        }

        if (enums.contains(param) || simpleEnums.contains(param)) {
          std::printf("spv::");
        }
        std::printf("%s arg%zu", param.c_str(), index++);
      }
      std::printf(") {\n");

      if (isBitEnum) {
        std::printf("    auto &operands = fields[Index];\n");
      } else {
        std::printf("    id = Id;\n");
      }

      for (std::size_t index = 0; auto &param : field.params) {
        if (enums.contains(param)) {
          std::printf("    arg%zu.forwardOperands(operands);\n", index++);
        } else if (simpleEnums.contains(param)) {
          std::printf(
              "    operands.push_back(static_cast<std::uint32_t>(arg%zu));\n",
              index++);
        } else {
          std::printf("    operands.push_back(arg%zu);\n", index++);
        }
      }
      std::printf("  }\n");
      std::printf("};\n");
    }
    std::printf("\n");
  }

  generateInstructions(composites, enums, coreJson, "Spv");

  std::printf("} //namespace shader::ir::spv\n");
  if (!glsl450.is_null()) {
    std::printf("namespace shader::ir::glsl450 {\n");
    std::printf("using namespace shader::ir::spv;\n");
    generateInstructions(composites, enums, glsl450, "Glsl450");
    std::printf("} //namespace shader::ir::glsl450\n");
  }
}

int main(int argc, const char *argv[]) {
  if (argc > 1) {
    int outFd = ::open(argv[1], O_CREAT | O_RDWR | O_TRUNC, 0666);
    ::dup2(outFd, 1);
    ::close(outFd);
  }

  json coreGrammar;
  json std450Grammar;
  std::ifstream("spirv.core.grammar.json") >> coreGrammar;
  //   std::ifstream("extinst.glsl.std.450.grammar.json") >> std450Grammar;
  generateGrammar(coreGrammar, std450Grammar);
  std::fflush(stdout);
  ::close(1);
}
