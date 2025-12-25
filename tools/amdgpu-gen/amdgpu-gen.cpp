#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <format>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <print>
#include <pugixml.hpp>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

static void usage(std::FILE *out, const char *argv0) {
  std::println(out, "{} <path to isa.xml> <--hpp|--cpp|--glsl>", argv0);
}

struct BitRange {
  std::uint32_t bitCount;
  std::uint32_t bitOffset;
  std::uint32_t paddingSize;
  std::uint32_t paddingValue;
};

struct PredefinedValue {
  std::string name;
  std::string description;
  std::uint32_t value;
};

struct MicrocodeField {
  std::string name;
  std::string description;
  std::vector<BitRange> bitLayout;
  std::vector<PredefinedValue> predefinedValues;
  bool isConditional;
  bool isUsed = false;
};

using Identifier = std::array<std::uint32_t, 5>;

struct EncodingCondition {
  std::string name;
  pugi::xml_node expression;
};

struct Encoding {
  std::string name;
  std::string description;
  std::uint32_t bitCount;
  Identifier mask;
  std::vector<Identifier> identifiers;
  std::vector<MicrocodeField> microcodeFormat;
  std::vector<EncodingCondition> encodingConditions;
};

enum class Signedness {
  DoesNotApply,
  Signed,
  Unsigned,
  SignedByModifier,
};

struct DataFormatField {
  std::string name;
  Signedness signedness;
  std::vector<BitRange> bitLayout;
};

enum class DataType {
  Bits,
  Integer,
  Float,
  Descriptor,
};

struct DataFormat {
  std::string name;
  std::string description;
  DataType dataType;
  std::uint32_t bitCount;
  std::uint32_t componentCount;
  std::vector<std::vector<DataFormatField>> fields;
  bool isUsed = false;
};

struct OperandType {
  std::string name;
  std::string description;
  std::vector<MicrocodeField> microcodeFormat;
  std::vector<PredefinedValue> predefinedValues;
  bool isUsed = false;
  bool isNotSideEffect = false;
};

struct Operand {
  bool isInput;
  bool isOutput;
  bool isImplicit;
  bool IsBinaryMicrocodeRequired;
  std::string fieldName;
  std::string dataFormatName;
  std::string type;
  std::uint32_t size;
};

struct InstructionEncoding {
  std::string name;
  std::string condition;
  std::uint32_t opcode;
  std::vector<Operand> operands;
};

struct Instruction {
  bool isBranch;
  bool IsConditionalBranch;
  bool IsIndirectBranch;
  bool IsProgramTerminator;
  bool IsImmediatelyExecuted;
  std::string name;
  std::string description;
  std::vector<InstructionEncoding> encodings;
  std::string functionalGroup;
  std::string functionalSubgroup;
};

struct Isa {
  std::string archName;
  std::vector<Encoding> encodings;
  std::vector<DataFormat> dataFormats;
  std::vector<Instruction> instructions;
  std::vector<OperandType> operandTypes;
  std::map<std::string, std::string> functionalGroups;
  std::vector<std::string> functionalSubgroups;
  std::vector<std::string> encodingGroups;
  std::map<std::string, std::string> encodingSubgroups;
};

static std::string toLower(std::string_view string) {
  return std::ranges::transform_view(string,
                                     [](char c) { return ::tolower(c); }) |
         std::ranges::to<std::string>();
}

static std::uint32_t getLayoutBitCount(std::span<const BitRange> bitLayout) {
  std::uint32_t result = 0;
  for (auto range : bitLayout) {
    result += range.bitCount + range.paddingSize;
  }
  return result;
}

static Identifier parseIdentifier(pugi::xml_node node, std::uint32_t bitCount) {
  auto radix = node.attribute("Radix").as_int(10);
  auto value = std::string_view(node.text().as_string());

  auto it = value.data();
  auto end = value.data() + value.size();
  auto wordCount = ((bitCount + 7) / 8 + 3) / 4;

  Identifier result;

  if (wordCount > result.size()) {
    std::println(stderr, "out of identifier limit, {}", bitCount);
    std::abort();
  }

  std::uint32_t count = 0;
  while (it != end) {
    if (count >= result.size()) {
      std::println(stderr, "out of identifier limit, {}", bitCount);
      std::abort();
    }

    auto last = end;
    if (radix == 2) {
      last = std::min(it + 32, end);
    }

    std::uint32_t word;
    auto [ptr, ec] = std::from_chars(it, last, word, radix);
    if (ec != std::errc{}) {
      std::println(stderr, "failed to parse identifier {}", value);
      std::abort();
    }

    result[wordCount - ++count] = word;
    it = ptr;
  }

  if (count != wordCount) {
    std::println(stderr,
                 "failed to parse value {}, radix {}, bit count {}, parsed "
                 "words {}, expected words {}",
                 value, radix, bitCount, count, wordCount);
    std::abort();
  }

  return result;
}

static std::vector<BitRange> parseBitLayout(pugi::xml_node xml) {
  std::vector<BitRange> result;
  for (auto range : xml.children("Range")) {
    BitRange newRange;
    newRange.bitCount = range.child("BitCount").text().as_int();
    newRange.bitOffset = range.child("BitOffset").text().as_int();
    if (auto padding = range.child("Padding")) {
      newRange.paddingSize = padding.child("BitCount").text().as_int();
      newRange.paddingValue =
          parseIdentifier(padding.child("Value"), newRange.paddingSize)[0];
    } else {
      newRange.paddingSize = 0;
      newRange.paddingValue = 0;
    }
    result.push_back(newRange);
  }

  return result;
}

static PredefinedValue parsePredefinedValue(pugi::xml_node xml) {
  PredefinedValue result;
  result.name = xml.child_value("Name");
  result.description = xml.child_value("Description");
  result.value = xml.child("Value").text().as_int();
  return result;
}

static MicrocodeField parseMicrocodeField(pugi::xml_node xml) {
  MicrocodeField result;
  result.name = xml.child_value("FieldName");
  result.description = xml.child_value("Description");
  result.bitLayout = parseBitLayout(xml.child("BitLayout"));

  if (auto predefinedValues = xml.child("FieldPredefinedValues")) {
    for (auto predefinedValue : predefinedValues.children("PredefinedValue")) {
      result.predefinedValues.push_back(parsePredefinedValue(predefinedValue));
    }
  }

  result.isConditional = xml.attribute("IsConditional").as_bool();
  return result;
}

static Encoding parseEncoding(pugi::xml_node xml) {
  Encoding result;
  result.name = xml.child_value("EncodingName");
  result.description = xml.child_value("Description");
  result.bitCount = xml.child("BitCount").text().as_int();

  result.mask =
      parseIdentifier(xml.child("EncodingIdentifierMask"), result.bitCount);

  for (auto ident :
       xml.child("EncodingIdentifiers").children("EncodingIdentifier")) {
    result.identifiers.push_back(parseIdentifier(ident, result.bitCount));
  }

  for (auto field :
       xml.child("MicrocodeFormat").child("BitMap").children("Field")) {
    result.microcodeFormat.push_back(parseMicrocodeField(field));
  }

  if (auto conditions = xml.child("EncodingConditions")) {
    for (auto condition : conditions.children("EncodingCondition")) {
      result.encodingConditions.push_back({
          .name = condition.child_value("ConditionName"),
          .expression =
              condition.child("CondtionExpression").child("Expression"),
      });
    }
  }

  return result;
}

static DataFormatField parseDataFormatField(pugi::xml_node xml) {
  DataFormatField result;
  result.name = xml.child_value("FieldName");
  auto signedness = std::string_view(xml.attribute("Signedness").as_string());
  if (signedness == "DoesNotApply") {
    result.signedness = Signedness::DoesNotApply;
  } else if (signedness == "Signed") {
    result.signedness = Signedness::Signed;
  } else if (signedness == "Unsigned") {
    result.signedness = Signedness::Unsigned;
  } else if (signedness == "SignedByModifier") {
    result.signedness = Signedness::SignedByModifier;
  } else {
    std::println(stderr, "unknown data format field signedness {}", signedness);
    std::abort();
  }

  result.bitLayout = parseBitLayout(xml.child("BitLayout"));
  return result;
}

static DataFormat parseDataFormat(pugi::xml_node xml) {
  DataFormat result{};

  result.name = xml.child_value("DataFormatName");
  result.description = xml.child_value("Description");
  auto dataType = std::string_view(xml.child_value("DataType"));
  if (dataType == "bits") {
    result.dataType = DataType::Bits;
  } else if (dataType == "integer") {
    result.dataType = DataType::Integer;
  } else if (dataType == "float") {
    result.dataType = DataType::Float;
  } else if (dataType == "descriptor") {
    result.dataType = DataType::Descriptor;
  } else {
    std::println(stderr, "unknown data format type {}", dataType);
    std::abort();
  }

  result.bitCount = xml.child("BitCount").text().as_int();
  result.componentCount = xml.child("ComponentCount").text().as_int();
  for (auto componentFields : xml.child("DataAttributes").children("BitMap")) {
    auto &resultFields = result.fields.emplace_back();

    for (auto field : componentFields.children("Field")) {
      resultFields.push_back(parseDataFormatField(field));
    }
  }
  return result;
}

static Operand parseOperand(pugi::xml_node xml) {
  Operand result;
  result.isInput = xml.attribute("Input").as_bool();
  result.isOutput = xml.attribute("Output").as_bool();
  result.isImplicit = xml.attribute("Implicit").as_bool();
  result.IsBinaryMicrocodeRequired =
      xml.attribute("BinaryMicrocodeRequired").as_bool();
  result.fieldName = xml.child_value("FieldName");
  result.dataFormatName = xml.child_value("DataFormatName");
  result.type = xml.child_value("OperandType");
  result.size = xml.child("OperandSize").text().as_int();
  return result;
}

static InstructionEncoding parseInstructionEncoding(pugi::xml_node xml) {
  InstructionEncoding result;
  result.name = xml.child_value("EncodingName");
  result.condition = xml.child_value("EncodingCondition");
  result.opcode = parseIdentifier(xml.child("Opcode"), 32)[0];
  for (auto operand : xml.child("Operands").children("Operand")) {
    auto parsed = parseOperand(operand);

    auto it = std::ranges::find_if(result.operands, [&](const Operand &exists) {
      return exists.size == parsed.size && exists.type == parsed.type &&
             exists.dataFormatName == parsed.dataFormatName &&
             exists.fieldName == parsed.fieldName;
    });

    if (it != result.operands.end()) {
      if (parsed.isInput) {
        it->isInput = true;
      }

      if (parsed.isOutput) {
        it->isOutput = true;
      }

      if (!parsed.isImplicit) {
        it->isImplicit = false;
      }

      if (parsed.IsBinaryMicrocodeRequired) {
        it->IsBinaryMicrocodeRequired = true;
      }
      continue;
    }

    result.operands.push_back(parsed);
  }
  return result;
}

static Instruction parseInstruction(pugi::xml_node xml) {
  Instruction result;

  auto flags = xml.child("InstructionFlags");
  result.isBranch = flags.child("IsBranch").text().as_bool();
  result.IsConditionalBranch =
      flags.child("IsConditionalBranch").text().as_bool();
  result.IsIndirectBranch = flags.child("IsIndirectBranch").text().as_bool();
  result.IsProgramTerminator =
      flags.child("IsProgramTerminator").text().as_bool();
  result.IsImmediatelyExecuted =
      flags.child("IsImmediatelyExecuted").text().as_bool();
  result.name = xml.child_value("InstructionName");
  result.description = xml.child_value("Description");
  for (auto encoding :
       xml.child("InstructionEncodings").children("InstructionEncoding")) {
    result.encodings.push_back(parseInstructionEncoding(encoding));
  }

  auto funcGroup = xml.child("FunctionalGroup");
  result.functionalGroup = funcGroup.child_value("Name");
  result.functionalSubgroup = funcGroup.child_value("Subgroup");

  return result;
}

static OperandType parseOperandType(pugi::xml_node xml) {
  OperandType result;
  result.name = xml.child_value("OperandTypeName");
  result.description = xml.child_value("Description");
  if (xml.attribute("IsPartitioned").as_bool()) {
    for (auto field :
         xml.child("MicrocodeFormat").child("BitMap").children("Field")) {
      result.microcodeFormat.push_back(parseMicrocodeField(field));
    }
  } else {
    for (auto predefinedValue :
         xml.child("OperandPredefinedValues").children("PredefinedValue")) {
      result.predefinedValues.push_back(parsePredefinedValue(predefinedValue));
    }
  }
  return result;
}

static Isa parseIsa(pugi::xml_node xml) {
  Isa result;

  result.archName = xml.child("Architecture").child_value("ArchitectureName");
  if (result.archName.starts_with("AMD ")) {
    result.archName = result.archName.substr(4);
  }
  result.archName = toLower(result.archName);

  for (auto pos = result.archName.find(' '); pos != std::string::npos;
       pos = result.archName.find(' ')) {
    result.archName.erase(pos, 1);
  }

  for (auto encoding : xml.child("Encodings").children("Encoding")) {
    result.encodings.push_back(parseEncoding(encoding));
  }

  for (auto instruction : xml.child("Instructions").children("Instruction")) {
    result.instructions.push_back(parseInstruction(instruction));
  }

  for (auto dataFormat : xml.child("DataFormats").children("DataFormat")) {
    result.dataFormats.push_back(parseDataFormat(dataFormat));
  }

  for (auto operandType : xml.child("OperandTypes").children("OperandType")) {
    result.operandTypes.push_back(parseOperandType(operandType));
  }

  for (auto group : xml.child("FunctionalGroups").children("FunctionalGroup")) {
    auto name = group.child_value("Name");
    auto description = group.child_value("Description");
    result.functionalGroups[name] = description;
  }

  for (auto group :
       xml.child("FunctionalSubgroups").children("FunctionalSubgroup")) {
    result.functionalSubgroups.emplace_back(group.child_value("Name"));
  }

  for (auto &encoding : result.encodings) {
    if (encoding.name.starts_with("ENC_")) {
      result.encodingGroups.push_back(encoding.name.substr(4));
    } else {
      result.encodingSubgroups[encoding.name] =
          encoding.name.substr(0, encoding.name.find('_'));
    }
  }

  for (auto &inst : result.instructions) {
    for (auto &instEnc : inst.encodings) {
      std::string baseEncoding;
      if (instEnc.name.starts_with("ENC_")) {
        baseEncoding = instEnc.name.substr(4);
      } else {
        baseEncoding = result.encodingSubgroups.at(instEnc.name);
      }
      auto baseEncIt =
          std::ranges::find(result.encodings, std::string_view(baseEncoding),
                            [](const Encoding &enc) {
                              return std::string_view(enc.name).substr(4);
                            });
      if (baseEncIt == result.encodings.end()) {
        std::println(stderr,
                     "instruction {} references to undefined base encoding {}",
                     inst.name, instEnc.name);
        std::abort();
      }

      for (auto &op : instEnc.operands) {
        auto dataFormatIt = std::ranges::find(
            result.dataFormats, std::string_view(op.dataFormatName),
            [](const DataFormat &format) {
              return std::string_view(format.name);
            });

        if (dataFormatIt == result.dataFormats.end()) {
          std::println(stderr,
                       "instruction {} references to undefined data format {}",
                       inst.name, op.dataFormatName);
          std::abort();
        }

        auto typeIt =
            std::ranges::find(result.operandTypes, std::string_view(op.type),
                              [](const OperandType &type) {
                                return std::string_view(type.name);
                              });
        if (typeIt == result.operandTypes.end()) {
          std::println(stderr, "instruction {} references to undefined type {}",
                       inst.name, op.type);
          std::abort();
        }

        dataFormatIt->isUsed = true;
        typeIt->isUsed = true;

        if (!op.fieldName.empty()) {
          typeIt->isNotSideEffect = true;
        } else if (!op.isImplicit && op.type == "OPR_SIMM32") {
          std::println(
              stderr,
              "explicit SIMM32 operand without field name, instruction "
              "{}, type {}",
              inst.name, typeIt->name);
          op.fieldName = "SIMM32";
          typeIt->isNotSideEffect = true;
        }

        if (!op.fieldName.empty()) {
          auto microcodeFieldIt = std::ranges::find(
              baseEncIt->microcodeFormat, std::string_view(op.fieldName),
              [](MicrocodeField &microcode) {
                return std::string_view(microcode.name);
              });

          if (microcodeFieldIt != baseEncIt->microcodeFormat.end()) {
            microcodeFieldIt->isUsed = true;
          }
        }
      }
    }
  }

  return result;
}

static void printComment(std::string_view text, int level) {
  while (!text.empty()) {
    auto eolPos = text.find('\n');
    auto line = text.substr(0, eolPos);
    if (!line.empty()) {
      std::println("{}// {}", std::string(level * 2, ' '), line);
    }

    if (eolPos == std::string_view::npos) {
      break;
    }

    text = text.substr(eolPos + 1);
  }
}

static std::string emitVariableRead(std::uint32_t resultBitSize,
                                    std::string_view variableName,
                                    std::uint32_t variableBitSize,
                                    BitRange range, bool isGlsl = false) {
  auto result = std::string(variableName);
  bool requiresParens = false;
  if (range.bitOffset != 0) {
    result += std::format(" >> {}", range.bitOffset);
    requiresParens = true;
  }

  if (resultBitSize != range.bitCount || resultBitSize == 1) {
    if (requiresParens) {
      result = std::format("({})", result);
    }

    result += std::format(" & {:#x}", (1ull << range.bitCount) - 1);
    requiresParens = true;
  }

  if (resultBitSize == 1) {
    if (range.paddingSize != 0) {
      std::println(stderr, "unexpected padding for boolean value");
      std::abort();
    }

    if (requiresParens) {
      result = std::format("({})", result);
    }
    return std::format("{} != 0", result);
  }

  if (range.paddingSize) {
    if (requiresParens) {
      result = std::format("({})", result);
    }

    result = std::format("{} << {}", result, range.paddingSize);

    if (range.paddingValue != 0) {
      result = std::format("({}) | {:#x}", result, range.paddingValue);
    }
  }

  if (resultBitSize != variableBitSize) {
    if (isGlsl) {
      return std::format("uint{}_t({})", resultBitSize, result);
    }

    return std::format("static_cast<std::uint{}_t>({})", resultBitSize, result);
  }

  return result;
}

static std::string emitArrayRead(std::uint32_t resultBitSize,
                                 std::string_view arrayName,
                                 std::uint32_t itemBitSize, BitRange range) {
  auto elementIndex = range.bitOffset / itemBitSize;
  range.bitOffset %= itemBitSize;
  return emitVariableRead(resultBitSize,
                          std::format("{}[{}]", arrayName, elementIndex),
                          itemBitSize, range);
}

static std::string emitExpression(pugi::xml_node xml) {
  auto type = xml.attribute("Type").as_string();
  if (type == std::string_view("Operator")) {
    auto op = xml.child_value("Operator");

    if (op == std::string_view(".fieldderef")) {
      auto subexpressions = xml.child("Subexpressions").children("Expression");
      auto exprs = std::vector(subexpressions.begin(), subexpressions.end());

      auto it = subexpressions.begin();
      if (it->child_value("Label") == std::string_view("INST")) {
        return "";
      }

      if (exprs.size() != 3) {
        std::println(stderr, "unexpected .fieldderef subexpression count, {}",
                     exprs.size());
        std::abort();
      }

      auto fieldSize =
          exprs[2].child("ValueType").child("Size").text().as_int();

      if (fieldSize == 1) {
        return std::string("is") + exprs[1].child_value("Label") + "()";
      }

      return std::string("get") + exprs[1].child_value("Label") + "()";
    }

    auto subexpressions = xml.child("Subexpressions").children("Expression");
    auto exprs = std::vector(subexpressions.begin(), subexpressions.end());

    if (exprs.size() != 3 && exprs.size() != 2) {
      std::println(stderr, "unexpected {} subexpression count, {}", op,
                   exprs.size());
      std::abort();
    }

    return std::format("({} {} {})", emitExpression(exprs[0]), op,
                       emitExpression(exprs[1]));
  }

  if (type == std::string_view("Literal")) {
    auto value = std::string_view(xml.child_value("Value"));

    if (xml.child("ValueType").child("Size").text().as_int() == 1) {
      return value == "0" ? "false" : "true";
    }

    return std::string(value);
  }

  std::println(stderr, "unexpected expression type {}", type);
  std::abort();
}

enum class OutputType { Header, Source, Glsl };

int main(int argc, const char *argv[]) {
  if (argc < 3) {
    if (argc == 2) {
      if (argv[1] == std::string_view("-h") ||
          argv[1] == std::string_view("--help")) {
        usage(stdout, argv[0]);
        return 0;
      }
    }

    usage(stderr, argv[0]);
    return 1;
  }

  OutputType type;
  if (argv[2] == std::string_view("--glsl")) {
    type = OutputType::Glsl;
  } else if (argv[2] == std::string_view("--hpp")) {
    type = OutputType::Header;
  } else if (argv[2] == std::string_view("--cpp")) {
    type = OutputType::Source;
  } else {
    usage(stderr, argv[0]);
    return 1;
  }

  pugi::xml_document doc;
  if (!doc.load_file(argv[1])) {
    std::println(stderr, "failed to load {}", argv[1]);
    return 1;
  }

  auto isa = parseIsa(doc.child("Spec").child("ISA"));

  struct EncodingParamField {
    std::string name;
    std::uint32_t bitCount;
    std::uint32_t bitOffset;
    std::uint32_t typeSize;
  };
  struct EncodingParams {
    std::uint32_t typeSize;
    std::vector<EncodingParamField> fields;
  };
  std::map<std::string, EncodingParams> encodingParams;

  std::map<std::string, std::map<std::uint32_t, std::string>>
      groupToInstructionSet;

  for (auto &inst : isa.instructions) {
    for (auto &instEnc : inst.encodings) {
      auto group = instEnc.name;
      if (group.starts_with("ENC_")) {
        group = group.substr(4);
      } else {
        group = isa.encodingSubgroups.at(instEnc.name);
      }

      auto &groupSet = groupToInstructionSet[group];

      auto [it, inserted] = groupSet.insert({instEnc.opcode, inst.name});

      if (!inserted) {
        if (inst.name != it->second) {
          std::println(
              stderr,
              "encoding group changes instruction instruction {}, encoding {}",
              inst.name, instEnc.name);
        }
      }
    }
  }

  for (auto &enc : isa.encodings) {
    if (!enc.name.starts_with("ENC_")) {
      continue;
    }

    std::vector<MicrocodeField> params;
    std::uint64_t totalFlagsSize = 0;

    for (auto &field : enc.microcodeFormat) {
      if (field.name == "ENCODING" || field.name == "OP" ||
          field.name == "OPM") {
        continue;
      }

      if (!field.isUsed) {
        params.emplace_back(field);
        totalFlagsSize += getLayoutBitCount(field.bitLayout);
      }
    }

    if (!params.empty()) {
      auto &encParams = encodingParams[enc.name.substr(4)];
      std::uint32_t storageTypeSize;
      if (totalFlagsSize == 1) {
        storageTypeSize = 1;
      } else if (totalFlagsSize > 32) {
        storageTypeSize = 64;
      } else if (totalFlagsSize > 16) {
        storageTypeSize = 32;
      } else if (totalFlagsSize > 8) {
        storageTypeSize = 16;
      } else {
        storageTypeSize = 8;
      }

      encParams.typeSize = storageTypeSize;
      std::uint32_t bitOffset = 0;
      for (auto &param : params) {
        auto totalBitCount = getLayoutBitCount(param.bitLayout);

        if (totalBitCount == 1) {
          encParams.fields.emplace_back(EncodingParamField{
              .name = param.name,
              .bitCount = 1,
              .bitOffset = bitOffset,
              .typeSize = 1,
          });
          bitOffset += 1;
        } else {
          std::uint32_t fieldTypeSize;
          if (totalBitCount > 32) {
            fieldTypeSize = 64;
          } else if (totalBitCount > 16) {
            fieldTypeSize = 32;
          } else if (totalBitCount > 8) {
            fieldTypeSize = 16;
          } else {
            fieldTypeSize = 8;
          }

          encParams.fields.emplace_back(EncodingParamField{
              .name = param.name,
              .bitCount = totalBitCount,
              .bitOffset = bitOffset,
              .typeSize = fieldTypeSize,
          });

          bitOffset += totalBitCount;
        }
      }
    }
  }

  if (type == OutputType::Source || type == OutputType::Header) {
    if (type == OutputType::Header) {
      std::println("#pragma once");
    } else {
      std::println("#include \"{}.hpp\"", isa.archName);
    }

    std::println("#include <cstdint>");
    std::println("#include <span>");
    std::println("#include <cstring>");
    std::println("#include <ostream>");

    if (type == OutputType::Source) {
      std::println("#include <iostream>");
    }
    std::println("");
  }

  if (type == OutputType::Header) {
    std::println("namespace amdgpu::{} {{", isa.archName);

    std::println("");
    std::println("enum class OperandAccess : std::uint8_t {{");
    std::println("  None,");
    std::println("  Read,");
    std::println("  Write,");
    std::println("  ReadWrite,");
    std::println("}};");

    std::println("");
    std::println("enum class InstructionFlags : std::uint8_t {{");
    std::println("  None = 0,");
    std::println("  Branch = 1 << 0,");
    std::println("  ConditionalBranch = 1 << 1,");
    std::println("  IndirectBranch = 1 << 2,");
    std::println("  ProgramTerminator = 1 << 3,");
    std::println("  ImmediatelyExecuted = 1 << 4,");
    std::println("}};");

    std::println("constexpr inline InstructionFlags operator|(InstructionFlags "
                 "lhs, InstructionFlags rhs) {{");
    std::println(
        "  return static_cast<InstructionFlags>(static_cast<std::uint8_t>(lhs) "
        "| static_cast<std::uint8_t>(rhs));");
    std::println("}}");

    std::println("constexpr inline InstructionFlags operator&(InstructionFlags "
                 "lhs, InstructionFlags rhs) {{");
    std::println(
        "  return static_cast<InstructionFlags>(static_cast<std::uint8_t>(lhs) "
        "& static_cast<std::uint8_t>(rhs));");
    std::println("}}");

    std::println("");
    std::println("enum class InstructionKind : std::uint8_t {{");
    for (auto &kind : isa.encodingGroups) {
      std::println("  {},", kind);
    }
    std::println("}};");

    std::println("");
    std::println("enum class OperandType : std::uint8_t {{");
    for (auto &type : isa.operandTypes) {
      if (!type.isUsed) {
        continue;
      }

      auto name = type.name;
      if (name.starts_with("OPR_")) {
        name = name.substr(4);
      }
      if (type.description.empty()) {
        std::println("  {},", name);
      } else if (type.description.find('\n') != std::string::npos) {
        printComment(type.description, 1);
        std::println("  {},", name);
      } else {
        std::print("  {}, ", name);
        printComment(type.description, 0);
      }
    }
    std::println("}};");

    std::println("");
    std::println("enum class DataFormat : std::uint8_t {{");
    for (auto &format : isa.dataFormats) {
      if (!format.isUsed) {
        continue;
      }

      auto name = format.name;
      if (name.starts_with("FMT_")) {
        name = name.substr(4);
      }
      if (format.description.empty()) {
        std::println("  {},", name);
      } else if (format.description.find('\n') != std::string::npos) {
        printComment(format.description, 1);
        std::println("  {},", name);
      } else {
        std::print("  {}, ", name);
        printComment(format.description, 0);
      }
    }
    std::println("}};");

    for (auto &type : isa.operandTypes) {
      if (!type.isUsed || !type.isNotSideEffect) {
        continue;
      }

      printComment(type.description, 0);
      if (!type.predefinedValues.empty()) {
        std::println("");
        std::println("enum class {} {{", type.name);
        for (auto &value : type.predefinedValues) {
          std::string name = value.name;
          if (name == "0.15915494") {
            name = "r2p";
          } else if (name[0] == '-') {
            name[0] = '_';
            name.insert(1, "m");
          } else if (name[0] >= '0' && name[0] <= '9') {
            name.insert(0, "_");
          }
          if (auto dotPos = name.find('.'); dotPos != std::string::npos) {
            name[dotPos] = '_';
          }

          if (value.description.empty()) {
            std::println("  {} = {},", name, value.value);
          } else if (value.description.find('\n') != std::string::npos) {
            printComment(value.description, 1);
            std::println("  {} = {},", name, value.value);
          } else {
            std::print("  {} = {}, ", name, value.value);
            printComment(value.description, 0);
          }
        }

        std::println("}};");
        continue;
      }

      if (type.microcodeFormat.empty()) {
        continue;
      }

      std::println("");
      std::println("struct {} {{", type.name);

      std::uint32_t layoutSize = 0;
      for (auto &microcode : type.microcodeFormat) {
        for (auto bitRange : microcode.bitLayout) {
          layoutSize =
              std::max(layoutSize, bitRange.bitOffset + bitRange.bitCount);
        }
      }

      if (layoutSize > 0) {
        std::uint32_t partitionTypeSize;
        if (layoutSize > 64) {
          std::println(stderr, "too big layout {}", layoutSize);
          std::abort();
        } else if (layoutSize > 32) {
          partitionTypeSize = 64;
        } else if (layoutSize > 16) {
          partitionTypeSize = 32;
        } else if (layoutSize > 8) {
          partitionTypeSize = 16;
        } else {
          partitionTypeSize = 8;
        }

        std::println("  std::uint{}_t raw;", partitionTypeSize);

        for (auto &microcode : type.microcodeFormat) {
          auto totalBitCount = getLayoutBitCount(microcode.bitLayout);

          std::uint32_t typeSize = 32;
          if (totalBitCount > 64) {
            std::println(stderr, "too big field {}", totalBitCount);
            std::abort();
          } else if (totalBitCount > 32) {
            typeSize = 64;
          } else if (totalBitCount > 16) {
            typeSize = 32;
          } else if (totalBitCount > 8) {
            typeSize = 16;
          } else if (totalBitCount == 1) {
            typeSize = 1;
          } else {
            typeSize = 8;
          }

          if (!microcode.predefinedValues.empty()) {
            if (typeSize == 1) {
              typeSize = 8;
            }

            std::println("");

            std::println("  enum class {} : std::uint{}_t {{", microcode.name,
                         typeSize);

            for (auto &value : microcode.predefinedValues) {
              std::string name = value.name;

              if (value.description.empty()) {
                std::println("    {} = {},", name, value.value);
              } else if (value.description.find('\n') != std::string::npos) {
                printComment(value.description, 2);
                std::println("    {} = {},", name, value.value);
              } else {
                std::print("    {} = {}, ", name, value.value);
                printComment(value.description, 0);
              }
            }
            std::println("  }};");
          }

          std::println("");
          printComment(microcode.description, 1);

          if (typeSize == 1) {
            if (microcode.bitLayout.size() != 1) {
              std::abort();
            }

            std::println("  [[nodiscard]] bool is{}() const {{",
                         microcode.name);
            auto offset = microcode.bitLayout.back().bitOffset;
            std::println("    return ((raw >> {}) & 1) != 0;", offset);
          } else {
            if (microcode.predefinedValues.empty()) {
              std::println("  [[nodiscard]] std::uint{}_t get{}() const {{",
                           typeSize, microcode.name);
            } else {
              std::println("  [[nodiscard]] {} get{}() const {{",
                           microcode.name, microcode.name);
            }
            if (microcode.bitLayout.size() == 1) {
              auto &bitRange = microcode.bitLayout.back();

              if (bitRange.bitCount > layoutSize) {
                std::println(stderr, "unexpected field size {}",
                             bitRange.bitCount);
                std::abort();
              }

              if (microcode.predefinedValues.empty()) {
                std::println("    return {};",
                             emitVariableRead(typeSize, "raw",
                                              partitionTypeSize, bitRange));
              } else {
                std::println("    return static_cast<{}>({});", microcode.name,
                             emitVariableRead(typeSize, "raw",
                                              partitionTypeSize, bitRange));
              }
            } else if (microcode.predefinedValues.empty()) {
              std::println("    std::uint{}_t result = 0;", typeSize);

              std::uint32_t resultBitOffset = 0;
              for (auto bitRange : microcode.bitLayout) {
                if (bitRange.bitCount > 32) {
                  std::println(stderr, "unexpected field size {}",
                               bitRange.bitCount);
                  std::abort();
                }

                if (resultBitOffset) {
                  std::println("    result |= ({}) << {};",
                               emitVariableRead(typeSize, "raw",
                                                partitionTypeSize, bitRange),
                               resultBitOffset);
                } else {
                  std::println("    result |= {};",
                               emitVariableRead(typeSize, "raw",
                                                partitionTypeSize, bitRange));
                }

                resultBitOffset += bitRange.bitCount;
              }

              if (microcode.predefinedValues.empty()) {
                std::println("    return result;");
              } else {
                std::println("    return static_cast<{}>(result);",
                             microcode.name);
              }
            }
          }

          std::println("  }}");
        }
      }

      std::println("}};");
    }

    for (auto &enc : isa.encodings) {
      if (!enc.name.starts_with("ENC_")) {
        continue;
      }

      std::vector<MicrocodeField> params;
      std::uint64_t totalFlagsSize = 0;

      for (auto &field : enc.microcodeFormat) {
        if (field.name == "ENCODING" || field.name == "OP" ||
            field.name == "OPM") {
          continue;
        }

        if (!field.isUsed) {
          params.emplace_back(field);
          totalFlagsSize += getLayoutBitCount(field.bitLayout);
        }
      }

      if (!params.empty()) {
        if (totalFlagsSize > 64) {
          std::println(stderr, "too big storage for flags");
          std::abort();
        }

        std::uint32_t storageTypeSize;
        if (totalFlagsSize > 32) {
          storageTypeSize = 64;
        } else if (totalFlagsSize > 16) {
          storageTypeSize = 32;
        } else if (totalFlagsSize > 8) {
          storageTypeSize = 16;
        } else {
          storageTypeSize = 8;
        }

        std::println("");
        std::println("struct {}_PARAMS {{", enc.name.substr(4));
        std::println("  std::uint{}_t raw;", storageTypeSize);
        std::uint32_t layoutOffset = 0;
        std::string ctorArgs;
        std::string ctorBody;
        for (auto &param : params) {
          auto totalBitCount = getLayoutBitCount(param.bitLayout);
          std::uint32_t fieldTypeSize;
          if (totalBitCount > 32) {
            fieldTypeSize = 64;
          } else if (totalBitCount > 16) {
            fieldTypeSize = 32;
          } else if (totalBitCount > 8) {
            fieldTypeSize = 16;
          } else {
            fieldTypeSize = 8;
          }

          if (!ctorArgs.empty()) {
            ctorArgs += ", ";
          }

          std::println("");
          printComment(param.description, 1);

          if (totalBitCount == 1) {
            if (param.bitLayout.size() != 1) {
              std::abort();
            }

            ctorArgs += "bool ";
            ctorArgs += toLower(param.name);

            ctorBody += std::format("    raw |= ({} ? 1 : 0) << {};\n",
                                    toLower(param.name), layoutOffset);

            std::println("  [[nodiscard]] bool is{}() const {{", param.name);
            std::println("    return ((raw >> {}) & 1) != 0;", layoutOffset);
            layoutOffset += 1;
          } else {
            std::println("  [[nodiscard]] std::uint{}_t get{}() const {{",
                         fieldTypeSize, param.name);

            ctorArgs += std::format("std::uint{}_t ", fieldTypeSize);
            ctorArgs += toLower(param.name);

            if (param.bitLayout.size() == 1) {
              auto bitRange = param.bitLayout.back();
              bitRange.bitOffset = layoutOffset;

              if (bitRange.bitCount > storageTypeSize) {
                std::println(stderr, "unexpected field size {}",
                             bitRange.bitCount);
                std::abort();
              }

              std::println("    return {};",
                           emitVariableRead(storageTypeSize, "raw",
                                            fieldTypeSize, bitRange));

              ctorBody += std::format(
                  "    raw |= ({} & {:#x}) << {};\n", toLower(param.name),
                  (1ull << bitRange.bitCount) - 1, layoutOffset);

              layoutOffset += totalBitCount;
            } else {
              std::println("    std::uint{}_t result = 0;", fieldTypeSize);

              std::uint32_t resultBitOffset = 0;
              for (auto bitRange : param.bitLayout) {
                bitRange.bitOffset = layoutOffset;

                if (bitRange.bitCount > 32) {
                  std::println(stderr, "unexpected field size {}",
                               bitRange.bitCount);
                  std::abort();
                }

                if (resultBitOffset) {
                  std::println("    result |= ({}) << {};",
                               emitVariableRead(storageTypeSize, "raw",
                                                fieldTypeSize, bitRange),
                               resultBitOffset);
                } else {
                  std::println("    result |= {};",
                               emitVariableRead(storageTypeSize, "raw",
                                                fieldTypeSize, bitRange));
                }

                ctorBody += std::format(
                    "    raw |= ({} & {:#x}) << {};\n", toLower(param.name),
                    (1ull << bitRange.bitCount) - 1, layoutOffset);

                resultBitOffset += bitRange.bitCount;
                layoutOffset += bitRange.bitCount;
              }

              std::println("    return result;");
            }
          }

          std::println("  }}");
        }

        std::println("");
        std::println("  static {}_PARAMS Create({}) {{", enc.name.substr(4),
                     ctorArgs);
        std::println("    std::uint{}_t raw = 0;", storageTypeSize);
        std::print("{}", ctorBody);
        std::println("    return {{.raw = raw}};");
        std::println("  }}");
        std::println("}};");
      }
    }
    std::println("");
    std::println("struct Operand {{");
    std::println("  OperandType type;");
    std::println("  OperandAccess access;");
    std::println("  DataFormat format;");
    std::println("  std::uint32_t size;");
    std::println("  union {{");
    std::println("    std::uint32_t id;");
    for (auto &type : isa.operandTypes) {
      if (!type.isUsed || type.microcodeFormat.empty()) {
        continue;
      }

      auto fieldName = type.name.substr(4);
      std::println("    {} {};", type.name, fieldName);
    }
    std::println("  }};");

    for (auto &type : isa.operandTypes) {
      if (!type.isUsed) {
        continue;
      }
      auto fieldName = type.name.substr(4);
      bool isPartitioned = !type.microcodeFormat.empty();
      std::println("");
      std::print("  static Operand Create{}(OperandAccess access", fieldName);

      if (type.isNotSideEffect) {
        if (isPartitioned) {
          std::uint32_t layoutSize = 0;
          for (auto &microcode : type.microcodeFormat) {
            for (auto bitRange : microcode.bitLayout) {
              layoutSize =
                  std::max(layoutSize, bitRange.bitOffset + bitRange.bitCount);
            }
          }

          std::uint32_t partitionTypeSize;
          if (layoutSize > 32) {
            partitionTypeSize = 64;
          } else if (layoutSize > 16) {
            partitionTypeSize = 32;
          } else if (layoutSize > 8) {
            partitionTypeSize = 16;
          } else {
            partitionTypeSize = 8;
          }

          std::print(", std::uint{}_t raw", partitionTypeSize);
        } else {
          std::print(
              ", DataFormat format, std::uint32_t size, std::uint32_t id");
        }
      }

      std::println(") {{");
      std::println("    Operand result; ", type.name);
      std::println("    result.type = OperandType::{};", fieldName);
      std::println("    result.access = access;");
      if (type.isNotSideEffect) {
        if (isPartitioned) {
          // std::println("    static_assert(sizeof(raw) == sizeof({}));",
          //              type.name);
          std::println("    std::memcpy(&result.{}, &raw, sizeof(raw));",
                       fieldName);
        } else {
          std::println("    result.format = format;");
          std::println("    result.size = size;");
          std::println("    result.id = id;");
        }
      }
      std::println("    return result;");
      std::println("  }}");
    }

    for (auto &type : isa.operandTypes) {
      if (!type.isUsed) {
        continue;
      }

      auto fieldName = type.name.substr(4);

      if (!type.microcodeFormat.empty()) {
        std::println("  [[nodiscard]] const {} &getAs{}() const {{", type.name,
                     fieldName);
        std::println("    return {};", fieldName);
        std::println("  }}");
      } else if (type.isNotSideEffect && !type.predefinedValues.empty()) {
        std::println("  [[nodiscard]] {} getAs{}() const {{", type.name,
                     fieldName);
        std::println("    return static_cast<{}>(id);", type.name);
        std::println("  }}");
      }
    }

    std::println("  [[nodiscard]] bool isSideEffect() const {{");
    std::println("    switch (getType()) {{");
    for (auto &type : isa.operandTypes) {
      if (!type.isUsed) {
        continue;
      }

      std::println("    case OperandType::{}: return {};", type.name.substr(4),
                   type.isNotSideEffect ? "false" : "true");
    }
    std::println("    }}");
    std::println("    return false;");
    std::println("  }}");

    std::println("  [[nodiscard]] bool isPartitioned() const {{");
    std::println("    switch (getType()) {{");
    for (auto &type : isa.operandTypes) {
      if (!type.isUsed) {
        continue;
      }
      std::println("    case OperandType::{}: return {};", type.name.substr(4),
                   type.isNotSideEffect && !type.microcodeFormat.empty()
                       ? "true"
                       : "false");
    }
    std::println("    }}");
    std::println("    return false;");
    std::println("  }}");

    std::println("  [[nodiscard]] OperandType getType() const {{");
    std::println("    return type;");
    std::println("  }}");

    std::println("  [[nodiscard]] DataFormat getFormat() const {{");
    std::println("    return format;");
    std::println("  }}");

    std::println("  [[nodiscard]] std::uint32_t getSize() const {{");
    std::println("    return size;");
    std::println("  }}");

    std::println("  [[nodiscard]] std::uint32_t getId() const {{");
    std::println("    return id;");
    std::println("  }}");

    std::println("  [[nodiscard]] bool hasR() const {{");
    std::println("    return access == OperandAccess::Read || access == "
                 "OperandAccess::ReadWrite;");
    std::println("  }}");

    std::println("  [[nodiscard]] bool hasW() const {{");
    std::println("    return access == OperandAccess::Write || access == "
                 "OperandAccess::ReadWrite;");
    std::println("  }}");

    std::println("  [[nodiscard]] bool isR() const {{");
    std::println("    return access == OperandAccess::Read;");
    std::println("  }}");

    std::println("  [[nodiscard]] bool isW() const {{");
    std::println("    return access == OperandAccess::Write;");
    std::println("  }}");

    std::println("  [[nodiscard]] bool isRW() const {{");
    std::println("    return access == OperandAccess::ReadWrite;");
    std::println("  }}");

    std::println("  void print(std::ostream &os) const;");
    std::println("  void dump() const;");

    std::println("}};");

    std::uint32_t maxOperandCount = 0;

    for (auto &inst : isa.instructions) {
      for (auto &enc : inst.encodings) {
        if (enc.operands.size() > maxOperandCount) {
          maxOperandCount = enc.operands.size();
        }
      }
    }

    std::println("");
    std::println("struct Instruction {{");
    std::println("  std::uint16_t opcode;");
    std::println("  InstructionKind kind;");
    std::println("  std::uint8_t operandCount;");
    std::println("  union {{");
    for (auto &group : isa.encodingGroups) {
      if (encodingParams.contains(group)) {
        std::println("    {0}_PARAMS {0}_params;", group);
      }
    }
    std::println("  }};");

    std::println("  Operand operands[{}];", maxOperandCount);

    std::println("");
    for (auto &group : isa.encodingGroups) {
      std::print("  static Instruction Create{}(std::uint16_t opcode", group);

      auto paramsIt = encodingParams.find(group);
      if (paramsIt != encodingParams.end()) {
        for (auto &paramField : paramsIt->second.fields) {
          std::print(", {} {}",
                     paramField.typeSize == 1
                         ? "bool"
                         : std::format("std::uint{}_t", paramField.typeSize),
                     paramField.name);
        }
      }
      std::println(") {{");
      std::println("    Instruction result;");
      std::println("    result.opcode = opcode;");
      std::println("    result.kind = InstructionKind::{};", group);
      std::println("    result.operandCount = 0;");
      if (paramsIt != encodingParams.end()) {
        std::print("    result.{0}_params = {0}_PARAMS::Create(", group);

        for (bool first = true; auto &field : paramsIt->second.fields) {
          if (first) {
            first = false;
          } else {
            std::print(", ");
          }

          std::print("{}", field.name);
        }

        std::println(");");
      }
      std::println("    return result;");
      std::println("  }}");
      std::println("");
    }

    std::println("  void addOperand(Operand operand) {{");
    std::println("    operands[operandCount++] = operand;");
    std::println("  }}");
    std::println("");

    std::println(
        "  [[nodiscard, gnu::pure]] InstructionFlags getFlags() const;");
    std::println("  [[nodiscard]] bool isBranch() const {{");
    std::println("    return (getFlags() & InstructionFlags::Branch) == "
                 "InstructionFlags::Branch;");
    std::println("  }}");
    std::println("  [[nodiscard]] bool isConditionalBranch() const {{");
    std::println(
        "    return (getFlags() & InstructionFlags::ConditionalBranch) == "
        "InstructionFlags::ConditionalBranch;");
    std::println("  }}");
    std::println("  [[nodiscard]] bool isIndirectBranch() const {{");
    std::println("    return (getFlags() & InstructionFlags::IndirectBranch) "
                 "== InstructionFlags::IndirectBranch;");
    std::println("  }}");
    std::println("  [[nodiscard]] bool isProgramTerminator() const {{");
    std::println(
        "    return (getFlags() & InstructionFlags::ProgramTerminator) == "
        "InstructionFlags::ProgramTerminator;");
    std::println("  }}");
    std::println("  [[nodiscard]] bool isImmediatelyExecuted() const {{");
    std::println(
        "    return (getFlags() & InstructionFlags::ImmediatelyExecuted) == "
        "InstructionFlags::ImmediatelyExecuted;");
    std::println("  }}");
    std::println("  void print(std::ostream &os) const;");
    std::println("  void dump() const;");
    std::println("}};");

    std::println("");
    std::println("bool decode(Instruction &instruction, std::span<const "
                 "std::uint32_t> &words);");
    std::println("}} // namespace amdgpu::{}", isa.archName);

    return 0;
  }

  if (type == OutputType::Source) {
    std::println("using namespace amdgpu::{};", isa.archName);

    for (auto &[group, insts] : groupToInstructionSet) {
      std::println(
          "static const char *get{}OpcodeName(std::uint16_t opcode) {{", group);

      std::println("  switch (opcode) {{");

      for (auto &[opcode, name] : insts) {
        std::println("  case {}: return \"{}\";", opcode, toLower(name));
      }

      std::println("  default:");
      std::println("    return nullptr;");
      std::println("  }}");
      std::println("}}");
    }

    for (auto &encoding : isa.encodings) {
      std::println("");
      printComment(encoding.description, 0);
      std::println("struct {} {{", encoding.name);
      auto wordCount = (((encoding.bitCount + 7) / 8) + 3) / 4;
      std::println("  std::uint32_t raw[{}];", wordCount);

      std::println("");
      std::println("  [[nodiscard]] static bool test(std::uint32_t inst) {{");

      int nonZeroWords = 0;

      for (std::size_t i = 0; i < wordCount; ++i) {
        if (encoding.mask[i] == 0) {
          continue;
        }

        nonZeroWords++;
      }

      if (nonZeroWords == 1 && encoding.mask[0] != 0) {
        std::println("    switch (inst & 0x{:08x}) {{", encoding.mask[0]);
        for (auto &ident : encoding.identifiers) {
          std::println("    case 0x{:08x}:", ident[0]);
        }

        std::println("      return true;");
        std::println("    default:");
        std::println("      return false;");
        std::println("    }}");
      } else {
        std::println(stderr, "invalid instruction mask");
        std::abort();
      }

      std::println("  }}");
      for (auto microcode : encoding.microcodeFormat) {
        std::println("");
        printComment(microcode.description, 1);

        auto totalBitCount = getLayoutBitCount(microcode.bitLayout);

        std::uint32_t typeSize = 32;
        if (totalBitCount > 64) {
          std::println(stderr, "too big field {}", totalBitCount);
          std::abort();
        } else if (totalBitCount > 32) {
          typeSize = 64;
        } else if (totalBitCount > 16) {
          typeSize = 32;
        } else if (totalBitCount > 8) {
          typeSize = 16;
        } else if (totalBitCount == 1) {
          typeSize = 1;
        } else {
          typeSize = 8;
        }

        if (typeSize == 1) {
          if (microcode.bitLayout.size() != 1) {
            std::abort();
          }

          std::println("  [[nodiscard]] bool is{}() const {{", microcode.name);
          auto offset = microcode.bitLayout.back().bitOffset;
          std::println("    return ((raw[{}] >> {}) & 1) != 0;", offset / 32,
                       offset % 32);
        } else {
          std::println("  [[nodiscard]] std::uint{}_t get{}() const {{",
                       typeSize, microcode.name);
          if (microcode.bitLayout.size() == 1) {
            auto bitRange = microcode.bitLayout.back();

            if (bitRange.bitCount > 32) {
              std::println(stderr, "unexpected field size {}",
                           bitRange.bitCount);
              std::abort();
            }

            std::println("    return {};",
                         emitArrayRead(typeSize, "raw", 32, bitRange));
          } else {
            std::println("    std::uint{}_t result = 0;", typeSize);

            std::uint32_t resultBitOffset = 0;
            for (auto bitRange : microcode.bitLayout) {
              if (bitRange.bitCount > 32) {
                std::println(stderr, "unexpected field size {}",
                             bitRange.bitCount);
                std::abort();
              }

              if (resultBitOffset) {
                std::println("    result |= ({}) << {};",
                             emitArrayRead(typeSize, "raw", 32, bitRange),
                             resultBitOffset);
              } else {
                std::println("    result |= {};",
                             emitArrayRead(typeSize, "raw", 32, bitRange));
              }

              resultBitOffset += bitRange.bitCount;
            }
            std::println("    return result;");
          }
        }

        std::println("  }}");
      }

      for (auto &condition : encoding.encodingConditions) {
        std::println("");
        std::println("  [[nodiscard]] bool cond_{}() const {{", condition.name);
        std::println("    return {};", emitExpression(condition.expression));
        std::println("  }}");
      }

      if (encoding.encodingConditions.empty()) {
        std::println("");
        std::println("  [[nodiscard]] bool cond_default() const {{");
        std::println("    return true;");
        std::println("  }}");
      }
      std::println("}};");
    }

    for (auto &enc : isa.encodings) {
      auto name = enc.name;
      if (name.starts_with("ENC_")) {
        name = name.substr(4);
      }

      std::println("static bool decode{}(Instruction &inst, "
                   "std::span<const std::uint32_t> &words) {{",
                   name);

      std::println("  if (!{}::test(words[0])) return false;", enc.name);
      std::println("  if (words.size() < sizeof({}) / sizeof(std::uint32_t)) "
                   "return false;",
                   enc.name);
      std::println("  {} enc;", enc.name);
      std::println("  std::memcpy(enc.raw, words.data(), sizeof(enc.raw));",
                   enc.name);
      MicrocodeField *opcodeField = nullptr;
      bool hasOpcodeExtension = false;
      for (auto &microcode : enc.microcodeFormat) {
        if (microcode.name == "OP") {
          opcodeField = &microcode;
          continue;
        }

        if (microcode.name == "OPM") {
          hasOpcodeExtension = true;
        }
      }
      std::string_view group = enc.name;
      if (group.starts_with("ENC_")) {
        group.remove_prefix(4);
      } else {
        group = isa.encodingSubgroups.at(std::string(group));
      }

      auto paramsIt = encodingParams.find(std::string(group));

      if (opcodeField != nullptr) {
        if (hasOpcodeExtension) {
          std::println(
              "  switch (enc.getOP() | ((enc.isOPM() ? 1 : 0) << {})) {{",
              getLayoutBitCount(opcodeField->bitLayout));
        } else {
          std::println("  switch (enc.getOP()) {{");
        }
      }
      for (auto &inst : isa.instructions) {
        bool isFirstCond = true;
        for (auto &instEnc : inst.encodings) {
          if (instEnc.name != enc.name) {
            continue;
          }

          if (opcodeField != nullptr && isFirstCond) {
            std::println("  case {}: ", instEnc.opcode);
          }

          std::println("    {}if (enc.cond_{}()) {{",
                       isFirstCond ? "" : "else ", instEnc.condition);
          isFirstCond = false;
          std::print("      inst = Instruction::Create{}({}", group,
                     instEnc.opcode);

          if (paramsIt != encodingParams.end()) {
            for (auto &paramField : paramsIt->second.fields) {
              auto microcodeHasField = std::ranges::contains(
                  enc.microcodeFormat, name, [](const MicrocodeField &field) {
                    return std::string_view(field.name);
                  });

              if (paramField.typeSize == 1) {
                if (microcodeHasField) {
                  std::print(", enc.is{}()", name);
                } else {
                  std::print(", false");
                }
              } else {
                if (microcodeHasField) {
                  std::print(", enc.get{}()", name);
                } else {
                  std::print(", 0");
                }
              }
            }
          }
          std::print("); ");
          printComment(inst.name, 0);

          for (auto &op : instEnc.operands) {
            auto dataFormatIt = std::ranges::find(
                isa.dataFormats, std::string_view(op.dataFormatName),
                [](const DataFormat &format) {
                  return std::string_view(format.name);
                });

            if (dataFormatIt == isa.dataFormats.end()) {
              std::println(
                  "instruction {} references to undefined data format {}",
                  inst.name, op.dataFormatName);
              std::abort();
            }

            auto typeIt =
                std::ranges::find(isa.operandTypes, std::string_view(op.type),
                                  [](const OperandType &type) {
                                    return std::string_view(type.name);
                                  });
            if (typeIt == isa.operandTypes.end()) {
              std::println("instruction {} references to undefined type {}",
                           inst.name, op.type);
              std::abort();
            }

            auto operandTypeName = op.type.substr(4);
            std::string_view access = "OperandAccess::None";
            if (op.isInput && op.isOutput) {
              access = "OperandAccess::ReadWrite";
            } else if (op.isInput) {
              access = "OperandAccess::Read";
            } else if (op.isOutput) {
              access = "OperandAccess::Write";
            }

            if (typeIt->isNotSideEffect) {
              if (typeIt->microcodeFormat.empty()) {
                if (!op.fieldName.empty()) {
                  std::println("      "
                               "inst.addOperand(Operand::Create{}({}, "
                               "DataFormat::{}, {}, "
                               "enc.get{}()));",
                               operandTypeName, access,
                               op.dataFormatName.substr(4), op.size,
                               op.fieldName);
                } else {
                  std::println("      "
                               "inst.addOperand(Operand::Create{}({}, "
                               "DataFormat::{}, {}, "
                               "0));",
                               operandTypeName, access,
                               op.dataFormatName.substr(4), op.size);
                }
              } else {
                std::println(
                    "      "
                    "inst.addOperand(Operand::Create{}({}, enc.get{}()));",
                    operandTypeName, access, op.fieldName);
              }
            } else {
              std::println("      inst.addOperand(Operand::Create{}({}));",
                           operandTypeName, access);
            }
          }

          std::println("    }}");
        }

        if (!isFirstCond) {
          std::println("    else {{");
          std::println("      return false;");
          std::println("    }}");
          if (opcodeField != nullptr) {
            std::println("    break;");
          }
        }
      }
      if (opcodeField != nullptr) {
        std::println("  default:");
        std::println("    return false;");
        std::println("  }}");
      }

      std::println(
          "  words = words.subspan(sizeof({}) / sizeof(std::uint32_t));",
          enc.name);
      std::println("  return true;");
      std::println("}}");
    }

    std::println("");
    std::println(
        "void amdgpu::{}::Instruction::print(std::ostream &os) const {{",
        isa.archName);
    std::println("}}");

    std::println("");
    std::println("void amdgpu::{}::Operand::dump() const {{", isa.archName);
    std::println("  print(std::cerr);");
    std::println("  std::cerr << '\\n';");
    std::println("}}");

    std::println("");
    std::println("void amdgpu::{}::Operand::print(std::ostream &os) const {{",
                 isa.archName);
    std::println("}}");

    std::println("");
    std::println("void amdgpu::{}::Instruction::dump() const {{", isa.archName);
    std::println("  print(std::cerr);");
    std::println("  std::cerr << '\\n';");
    std::println("}}");

    std::println("");
    std::println("bool amdgpu::{}::decode(Instruction &inst, "
                 "std::span<const std::uint32_t> &words) {{",
                 isa.archName);
    for (auto &enc : isa.encodings) {
      auto name = enc.name;
      if (name.starts_with("ENC_")) {
        name = name.substr(4);
      }
      std::println("  if (decode{}(inst, words)) return true;", name);
    }
    std::println("  return false;");
    std::println("}}");

    return 0;
  }

  if (type == OutputType::Glsl) {
    nlohmann::json semanticDescriptions;
    if (argc > 3) {
      std::ifstream(argv[3]) >> semanticDescriptions;
    }
    std::println("#version 460");
    std::println(
        "#extension GL_EXT_shader_explicit_arithmetic_types : require");

    for (auto &[funcGroupName, funcGroupDescription] : isa.functionalGroups) {
      std::map<std::string, std::vector<Instruction *>> subgroupToInstruction;

      for (auto &inst : isa.instructions) {
        if (inst.functionalGroup != funcGroupName) {
          continue;
        }

        if (!inst.IsConditionalBranch && inst.isBranch) {
          continue;
        }

        if (inst.name == "EXP") {
          continue;
        }

        subgroupToInstruction[inst.functionalSubgroup].push_back(&inst);
      }

      if (subgroupToInstruction.empty()) {
        continue;
      }

      std::println("");
      std::println(
          "// =========================================================");
      printComment(funcGroupName, 0);
      printComment(funcGroupDescription, 0);
      std::println(
          "// =========================================================");

      for (auto &[subgroup, instList] : subgroupToInstruction) {
        if (!subgroup.empty()) {
          std::println("");
          std::println(
              "// ---------------------------------------------------------");
          printComment(subgroup, 0);
          std::println(
              "// ---------------------------------------------------------");
        }

        for (auto inst : instList) {
          std::println("");
          printComment(inst->description, 0);
          if (inst->IsConditionalBranch) {
            std::print("bool ");
          } else {
            std::print("void ");
          }
          std::print("{}(", toLower(inst->name));

          auto instEncIt = std::ranges::find_if(
              inst->encodings, [&](const InstructionEncoding &enc) {
                auto group = enc.name;
                if (group.starts_with("ENC_")) {
                  group = group.substr(4);
                } else {
                  group = isa.encodingSubgroups.at(enc.name);
                }

                return encodingParams.contains(std::string(group));
              });

          auto &instEnc = instEncIt == inst->encodings.end()
                              ? inst->encodings.front()
                              : *instEncIt;

          auto group = instEnc.name;
          if (group.starts_with("ENC_")) {
            group = group.substr(4);
          } else {
            group = isa.encodingSubgroups.at(instEnc.name);
          }

          auto paramsIt = encodingParams.find(std::string(group));

          for (bool first = true; auto &op : instEnc.operands) {
            if (first) {
              first = false;
            } else {
              std::print(", ");
            }

            auto dataFormatIt = std::ranges::find(
                isa.dataFormats, std::string_view(op.dataFormatName),
                [](const DataFormat &format) {
                  return std::string_view(format.name);
                });

            if (dataFormatIt == isa.dataFormats.end()) {
              std::println(
                  stderr,
                  "instruction {} references to undefined data format {}",
                  inst->name, op.dataFormatName);
              std::abort();
            }

            auto typeIt =
                std::ranges::find(isa.operandTypes, std::string_view(op.type),
                                  [](const OperandType &type) {
                                    return std::string_view(type.name);
                                  });
            if (typeIt == isa.operandTypes.end()) {
              std::println(stderr,
                           "instruction {} references to undefined type {}",
                           inst->name, op.type);
              std::abort();
            }

            if (op.isInput && op.isOutput) {
              std::print("inout ");
            } else if (op.isInput) {
              std::print("in ");
            } else if (op.isOutput) {
              std::print("out ");
            }

            std::uint32_t arrayCount = 1;

            auto selectElementType =
                [&](std::uint32_t componentSize, std::uint32_t componentCount,
                    bool isFloat, bool isSigned) -> std::string {
              if (componentCount > 4) {
                std::println(stderr, "unexpected component count {}",
                             componentCount);
                std::abort();
              }
              if (componentSize > 64) {
                std::println(stderr, "unexpected component size {}",
                             componentSize);
                std::abort();
              }

              if (componentCount == 1) {
                if (componentSize == 1) {
                  return "bool";
                }

                if (isFloat) {
                  return std::format("float{}_t", componentSize);
                }

                if (componentSize > 32) {
                  return std::format("{}int64_t", isSigned ? "" : "u");
                }

                if (componentSize > 16) {
                  return std::format("{}int32_t", isSigned ? "" : "u");
                }

                if (componentSize > 8) {
                  return std::format("{}int16_t", isSigned ? "" : "u");
                }

                return std::format("{}int8_t", isSigned ? "" : "u");
              }

              if (componentSize == 1) {
                return std::format("bvec{}", componentCount);
              }

              if (isFloat) {
                return std::format("f{}vec{}", componentSize, componentCount);
              }

              if (componentSize > 32) {
                return std::format("{}64vec{}", isSigned ? "i" : "u",
                                   componentCount);
              }

              if (componentSize > 16) {
                return std::format("{}32vec{}", isSigned ? "i" : "u",
                                   componentCount);
              }

              if (componentSize > 8) {
                return std::format("{}16vec{}", isSigned ? "i" : "u",
                                   componentCount);
              }

              return std::format("{}8vec{}", isSigned ? "i" : "u",
                                 componentCount);
            };

            auto selectTypeForBits =
                [&](std::uint32_t operandSize,
                    DataFormat *dataFormat) -> std::string {
              if (dataFormat && dataFormat->dataType != DataType::Descriptor) {
                bool isFloat = dataFormat->dataType == DataType::Float;
                bool isSigned =
                    dataFormat->dataType == DataType::Integer &&
                    dataFormat->fields[0][0].signedness == Signedness::Signed;

                if (operandSize < dataFormat->bitCount) {
                  std::println(
                      stderr, "unexpected operand size {}, data format size {}",
                      operandSize, dataFormat->bitCount);
                  std::abort();
                }

                auto elementCount = operandSize / dataFormat->bitCount;
                auto componentCount = dataFormat->componentCount;
                auto componentSize =
                    dataFormat->bitCount / dataFormat->componentCount;

                while (componentSize > 64) {
                  componentSize /= 2;
                  componentCount *= 2;
                }

                if (componentCount == 1 && elementCount <= 4) {
                  componentCount = elementCount;
                } else if (componentCount > 4 && dataFormat->bitCount <= 64) {
                  componentSize = dataFormat->bitCount;
                  componentCount = 1;
                } else if (componentCount > 4) {
                  arrayCount = componentCount * elementCount;
                  componentCount = 1;
                } else {
                  arrayCount = elementCount;
                }

                return selectElementType(componentSize, componentCount, isFloat,
                                         isSigned);
              }

              if (operandSize <= 64) {
                return selectElementType(operandSize, 1, false, false);
              }

              if (operandSize % 32) {
                std::println(stderr, "unexpected operand size {}", operandSize);
                std::abort();
              }

              auto elementCount = operandSize / 32;
              if (elementCount <= 4) {
                return std::format("u32vec{}", elementCount);
              }

              arrayCount = elementCount;
              return "uint32_t";
            };

            if (op.dataFormatName == "FMT_ANY") {
              std::print("{}", selectTypeForBits(op.size, nullptr));
            } else {
              std::print("{}", selectTypeForBits(op.size, &*dataFormatIt));
            }

            if (!op.fieldName.empty()) {
              std::print(" {}", toLower(op.fieldName));
            } else {
              if (op.type == "OPR_SSRC_SPECIAL_SCC") {
                std::print(" scc");
              } else {
                std::print(" {}", toLower(op.type.substr(4)));
              }
            }

            if (arrayCount != 1) {
              std::print("[{}]", arrayCount);
            }
          }

          if (paramsIt != encodingParams.end()) {
            if (!instEnc.operands.empty()) {
              std::print(", ");
            }

            std::print("in {} {}",
                       paramsIt->second.typeSize == 1
                           ? "bool"
                           : std::format("uint{}_t", paramsIt->second.typeSize),
                       paramsIt->second.fields.size() == 1
                           ? toLower(paramsIt->second.fields[0].name)
                           : "params");
          }

          std::println(") {{");
          if (semanticDescriptions.contains(inst->name)) {
            printComment(semanticDescriptions[inst->name].get<std::string>(),
                         1);
          }

          if (paramsIt != encodingParams.end() &&
              paramsIt->second.fields.size() != 1) {
            for (auto &field : paramsIt->second.fields) {
              std::println(
                  "  {} {} = {};",
                  field.typeSize == 1 ? "bool"
                                      : std::format("uint{}_t", field.typeSize),
                  toLower(field.name),
                  emitVariableRead(field.typeSize, "params", 32,
                                   {
                                       .bitCount = field.bitCount,
                                       .bitOffset = field.bitOffset,
                                   },
                                   true));
            }
          }

          if (inst->IsConditionalBranch) {
            std::println("  return false;");
          }
          std::println("}}");
        }
      }
    }
    return 0;
  }

  return 1;
}
