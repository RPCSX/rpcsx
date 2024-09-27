#pragma once

#include "ir/Context.hpp"
#include "ir/Region.hpp"
#include "ir/RegionImpl.hpp"
#include <optional>
#include <span>

namespace shader::spv {

struct BinaryLayout {
  enum {
    kCapabilities,
    kExtensions,
    kExtInstImports,
    kMemoryModels,
    kEntryPoints,
    kExecutionModes,
    kDebugStrings,
    kDebugs,
    kAnnotations,
    kGlobals,
    kFunctionDeclarations,
    kFunctions,

    kRegionCount
  };

  ir::Region regions[kRegionCount];

  ir::Region getOrCreateRegion(ir::Context &context, int index) {
    if (regions[index] == nullptr) {
      regions[index] = context.create<ir::Region>(context.getUnknownLocation());
    }

    return regions[index];
  }

  ir::Region getOrCreateCapabilities(ir::Context &context) {
    return getOrCreateRegion(context, kCapabilities);
  }
  ir::Region getOrCreateExtensions(ir::Context &context) {
    return getOrCreateRegion(context, kExtensions);
  }
  ir::Region getOrCreateExtInstImports(ir::Context &context) {
    return getOrCreateRegion(context, kExtInstImports);
  }
  ir::Region getOrCreateMemoryModels(ir::Context &context) {
    return getOrCreateRegion(context, kMemoryModels);
  }
  ir::Region getOrCreateEntryPoints(ir::Context &context) {
    return getOrCreateRegion(context, kEntryPoints);
  }
  ir::Region getOrCreateExecutionModes(ir::Context &context) {
    return getOrCreateRegion(context, kExecutionModes);
  }
  ir::Region getOrCreateDebugStrings(ir::Context &context) {
    return getOrCreateRegion(context, kDebugStrings);
  }
  ir::Region getOrCreateDebugs(ir::Context &context) {
    return getOrCreateRegion(context, kDebugs);
  }
  ir::Region getOrCreateAnnotations(ir::Context &context) {
    return getOrCreateRegion(context, kAnnotations);
  }
  ir::Region getOrCreateGlobals(ir::Context &context) {
    return getOrCreateRegion(context, kGlobals);
  }
  ir::Region getOrCreateFunctionDeclarations(ir::Context &context) {
    return getOrCreateRegion(context, kFunctionDeclarations);
  }
  ir::Region getOrCreateFunctions(ir::Context &context) {
    return getOrCreateRegion(context, kFunctions);
  }

  ///
  /// \brief Merge all regions into a single one.
  ///
  /// After calling this function, all regions in the object
  /// become empty.
  ///
  ir::Region merge(ir::Context &context) {
    auto result = context.create<ir::Region>(context.getUnknownLocation());
    for (auto &region : regions) {
      if (region == nullptr) {
        continue;
      }

      result.appendRegion(std::move(region));
      region = {};
    }

    return result;
  }
};

///
/// Deserialize a SPIR-V binary into an intermediate representation.
///
/// \param context context to attach the IR to
/// \param spv SPIR-V binary
/// \param loc location to use for error reporting
/// \returns the deserialized IR, or std::nullopt if deserialization failed
///
std::optional<BinaryLayout> deserialize(ir::Context &context,
                                        std::span<const std::uint32_t> spv,
                                        ir::Location loc);
///
/// \brief Serialize SPIR-V from an IR region.
///
/// This function generates a SPIR-V binary from an IR region.
/// The SPIR-V binary is stored in the returned vector.
///
/// \returns A vector of u32 values representing the SPIR-V binary.
///
std::vector<std::uint32_t> serialize(ir::Region body);

inline std::vector<std::uint32_t> serialize(ir::Context &context,
                                            BinaryLayout &&layout) {
  return serialize(layout.merge(context));
}

///
/// \brief Returns true if the instruction is a terminator.
///
bool isTerminatorInst(ir::InstructionId inst);

///
/// \brief Disassemble a SPIR-V binary into text and print result to stderr.
///
/// \param spv The SPIR-V binary to disassemble.
/// \param pretty If true, emit friendly names for functions, variables, and
/// other values.  If false, emit the SPIR-V ID for each value.
///
/// \note The SPIR-V binary is not validated or checked for errors.  If the
/// input is invalid, the output is undefined.
void dump(std::span<const std::uint32_t> spv, bool pretty = false);

///
/// \brief Disassemble a SPIR-V binary into text.
///
/// \param spv The SPIR-V binary to disassemble.
/// \param pretty If true, emit friendly names for functions, variables, and
/// other values.  If false, emit the SPIR-V ID for each value.
/// \return the assembly text
///
/// \note The SPIR-V binary is not validated or checked for errors.  If the
/// input is invalid, the output is undefined.
std::string disassembly(std::span<const std::uint32_t> spv, bool pretty = false);

///
/// \brief Validates a given SPIR-V binary against the SPIR-V spec
///
/// \param spv the SPIR-V binary to validate
/// \return whether the SPIR-V binary is valid
///
/// This functions uses the SPIR-V Tools validator to check the given SPIR-V
/// binary against the SPIR-V spec. If the SPIR-V is invalid, the function
/// will print out the validation error messages and return false. If the
/// SPIR-V is valid, the function simply returns true.
bool validate(std::span<const std::uint32_t> spv);

///
/// \brief Optimize a SPIR-V module.
///
/// \param spv the SPIR-V binary to optimize
/// \return the optimized SPIR-V binary or an empty optional if binary is
/// invalid
///
/// This function takes a SPIR-V module and runs a series of optimization passes
/// on it using SPIR-V Tools opt.  If the optimization is successful, the
/// optimized module is returned. Otherwise, an empty optional is returned.
///
std::optional<std::vector<std::uint32_t>>
optimize(std::span<const std::uint32_t> spv);
} // namespace shader::spv
