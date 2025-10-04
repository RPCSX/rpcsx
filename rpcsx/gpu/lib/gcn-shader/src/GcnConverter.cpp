#include "GcnConverter.hpp"
#include "ModuleInfo.hpp"
#include "SPIRV/GLSL.std.450.h"
#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "dialect.hpp"
#include "gcn.hpp"
#include "ir.hpp"
#include "rx/die.hpp"
#include "rx/print.hpp"
#include <iostream>
#include <limits>

using namespace shader;

inline constexpr auto kConfigBinding = 0;

struct GcnConverter {
  gcn::Context &gcnContext;
  ir::Value configBuffer;

  ir::Value getGlPosition(gcn::Builder &builder);
  ir::Value getConfigBlock(int descriptorSet);
  ir::Value createReadConfig(gcn::Stage stage, gcn::Builder &builder,
                             const ir::Operand &index);

  ir::Value createLocalVariable(gcn::Builder &builder, ir::Location loc,
                                ir::Value initializer,
                                ir::Value type = nullptr) {
    if (type == nullptr) {
      type = initializer.getOperand(0).getAsValue();
    } else if (type != initializer.getOperand(0).getAsValue()) {
      initializer = builder.createSpvBitcast(loc, type, initializer);
    }
    auto variableType =
        gcnContext.getTypePointer(ir::spv::StorageClass::Function, type);
    auto result =
        gcn::Builder::createAppend(gcnContext, gcnContext.localVariables)
            .createSpvVariable(loc, variableType,
                               ir::spv::StorageClass::Function);
    builder.createSpvStore(loc, result, initializer);
    return result;
  };
};

inline int stageToDescriptorSet(gcn::Stage stage) {
  switch (stage) {
  case gcn::Stage::Ps:
    return 2;
  case gcn::Stage::VsVs:
    return 0;
  case gcn::Stage::Cs:
    return 0;
  case gcn::Stage::Gs:
    return 1;
  case gcn::Stage::GsVs:
    return 0;
  case gcn::Stage::DsVs:
    return 0;

  case gcn::Stage::VsEs:
  case gcn::Stage::VsLs:
  case gcn::Stage::Hs:
  case gcn::Stage::DsEs:
  case gcn::Stage::Invalid:
    break;
  }

  std::abort();
}

static void printFlat(std::ostream &os, ir::Instruction inst,
                      ir::NameStorage &ns) {
  if (inst == nullptr) {
    os << "null";
    return;
  }
  os << ir::getInstructionName(inst.getKind(), inst.getOp());
  os << '(';

  for (bool first = true; auto &op : inst.getOperands()) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }

    if (auto valueOp = op.getAsValue()) {
      printFlat(os, valueOp, ns);
    } else {
      op.print(os, ns);
    }
  }

  os << ')';
}

static bool isEqual(ir::Value lhs, ir::Value rhs);

static bool isEqual(const ir::Operand &lhs, const ir::Operand &rhs) {
  return std::visit(
      []<typename Lhs, typename Rhs>(const Lhs &lhs, const Rhs &rhs) {
        if constexpr (std::is_same_v<Lhs, Rhs>) {
          if constexpr (std::is_same_v<Lhs, ir::Value>) {
            return isEqual(lhs, rhs);
          } else {
            return lhs == rhs;
          }
        } else {
          return false;
        }
      },
      lhs.value, rhs.value);
}

static bool isEqual(ir::Value lhs, ir::Value rhs) {
  if (lhs == rhs) {
    return true;
  }

  if ((lhs == nullptr) != (rhs == nullptr)) {
    return false;
  }

  if (lhs.getInstId() != rhs.getInstId()) {
    return false;
  }

  if (lhs.getOperandCount() != rhs.getOperandCount()) {
    return false;
  }

  for (std::size_t i = 0, end = lhs.getOperandCount(); i < end; ++i) {
    if (!isEqual(lhs.getOperand(i), rhs.getOperand(i))) {
      return false;
    }
  }

  return true;
}

struct ResourcesBuilder {
  gcn::Resources resources;
  ir::NameStorage *ns;

  int addPointer(gcn::Resources::Pointer p) {
    for (auto &pointer : resources.pointers) {
      if (pointer.size != p.size) {
        continue;
      }

      if (!isEqual(pointer.base, p.base)) {
        continue;
      }

      if (!isEqual(pointer.offset, p.offset)) {
        continue;
      }

      return pointer.resourceSlot;
    }

    p.resourceSlot = resources.slots++;
    resources.pointers.push_back(p);
    return p.resourceSlot;
  }
  int addTexture(gcn::Resources::Texture p) {
    for (auto &texture : resources.textures) {
      bool equal = true;

      for (std::size_t i = 0; i < std::size(texture.words); ++i) {
        if (!isEqual(texture.words[i], p.words[i])) {
          equal = false;
          break;
        }
      }

      if (!equal) {
        continue;
      }

      texture.access |= p.access;
      return texture.resourceSlot;
    }

    p.resourceSlot = resources.slots++;
    resources.textures.push_back(p);
    return p.resourceSlot;
  }
  int addImageBuffer(gcn::Resources::ImageBuffer p) {
    for (auto &buffer : resources.imageBuffers) {
      bool equal = true;

      for (std::size_t i = 0; i < std::size(buffer.words); ++i) {
        if (!isEqual(buffer.words[i], p.words[i])) {
          equal = false;
          break;
        }
      }

      if (!equal) {
        continue;
      }

      buffer.access |= p.access;
      return buffer.resourceSlot;
    }

    p.resourceSlot = resources.slots++;
    resources.imageBuffers.push_back(p);
    return p.resourceSlot;
  }
  int addBuffer(gcn::Resources::Buffer p) {
    for (auto &buffer : resources.buffers) {
      bool equal = true;

      for (std::size_t i = 0; i < std::size(buffer.words); ++i) {
        if (!isEqual(buffer.words[i], p.words[i])) {
          equal = false;
          break;
        }
      }

      if (!equal) {
        continue;
      }

      buffer.access |= p.access;
      return buffer.resourceSlot;
    }

    p.resourceSlot = resources.slots++;
    resources.buffers.push_back(p);
    return p.resourceSlot;
  }
  int addSampler(gcn::Resources::Sampler p) {
    for (auto &sampler : resources.samplers) {
      bool equal = true;

      for (std::size_t i = 0; i < std::size(sampler.words); ++i) {
        if (!isEqual(sampler.words[i], p.words[i])) {
          equal = false;
          break;
        }
      }

      if (!equal) {
        continue;
      }

      return sampler.resourceSlot;
    }

    p.resourceSlot = resources.slots++;
    resources.samplers.push_back(p);
    return p.resourceSlot;
  }

  ir::Value unpackFunctionCall(MemorySSA &memorySSA, spv::Import &importer,
                               ir::Value call) {
    for (auto &argOp : call.getOperands().subspan(2)) {
      auto argValue = argOp.getAsValue();

      if (argValue == ir::spv::OpVariable ||
          argValue == ir::spv::OpAccessChain) {
        auto varDef = memorySSA.getDefInst(call, argValue);
        if (varDef == ir::spv::OpStore) {
          varDef = varDef.getOperand(1).getAsValue();
        }
        if (varDef == ir::amdgpu::POINTER) {
          return importIR(memorySSA, importer, varDef).staticCast<ir::Value>();
        }
      } else if (argValue == ir::amdgpu::POINTER) {
        return importIR(memorySSA, importer, argValue).staticCast<ir::Value>();
      }
    }

    std::printf("failed to resolve function call to %s\n",
                ns->getNameOf(call.getOperand(1).getAsValue()).c_str());

    for (auto &op : call.getOperands().subspan(2)) {
      std::cerr << "arg: ";
      op.print(std::cerr, *ns);
      auto argValue = op.getAsValue();

      if (argValue == ir::spv::OpVariable ||
          argValue == ir::spv::OpAccessChain) {
        auto varDef = memorySSA.getDefInst(call, argValue);
        if (varDef == ir::spv::OpStore) {
          varDef = varDef.getOperand(1).getAsValue();
        }
        if (varDef) {
          std::cerr << " def is ";
          varDef.print(std::cerr, *ns);
        } else {
          std::cerr << " def is null";
        }
      }
      std::cerr << "\n";
    }

    resources.hasUnknown = true;
    return nullptr;
  }

  ir::Instruction unpackInstruction(MemorySSA &memorySSA, spv::Import &importer,
                                    ir::Instruction inst) {
    for (auto &op : inst.getOperands()) {
      auto value = op.getAsValue();
      if (!value) {
        continue;
      }

      if (value == ir::spv::OpVariable || value == ir::spv::OpAccessChain) {
        auto varDef = memorySSA.getDefInst(inst, value);
        if (varDef == ir::spv::OpStore) {
          varDef = varDef.getOperand(1).getAsValue();
        }
        if (varDef == ir::amdgpu::POINTER) {
          return importIR(memorySSA, importer, varDef).staticCast<ir::Value>();
        }
      } else if (value == ir::amdgpu::POINTER) {
        return importIR(memorySSA, importer, value).staticCast<ir::Value>();
      }
    }

    return inst;
  }

  ir::Instruction unpackResourceDef(MemorySSA &memorySSA, spv::Import &importer,
                                    ir::memssa::Def def) {
    if (def == nullptr) {
      return nullptr;
    }

    if (auto defInst = def.getLinkedInst()) {
      if (defInst == ir::spv::OpStore) {
        return importIR(memorySSA, importer,
                        defInst.getOperand(1).getAsValue());
      }

      if (defInst == ir::spv::OpFunctionCall) {
        return unpackFunctionCall(memorySSA, importer,
                                  defInst.staticCast<ir::Value>());
      }

      if (defInst.getKind() != ir::Kind::Spv &&
          defInst.getKind() != ir::Kind::AmdGpu) {
        return unpackInstruction(memorySSA, importer, defInst);
      }

      return importIR(memorySSA, importer, defInst);
    }

    if (auto phi = def.cast<ir::memssa::Phi>()) {
      auto resourcePhi = resources.context.create<ir::Value>(
          phi.getLocation(), ir::Kind::AmdGpu, ir::amdgpu::RESOURCE_PHI);

      for (std::size_t i = 1, end = phi.getOperandCount(); i < end; i += 2) {
        auto pred =
            phi.getOperand(i).getAsValue().staticCast<ir::memssa::Scope>();
        auto def =
            phi.getOperand(i + 1).getAsValue().staticCast<ir::memssa::Def>();

        auto inst = unpackResourceDef(memorySSA, importer, def);

        if (inst == nullptr) {
          resources.hasUnknown = true;
        }

        resourcePhi.addOperand(pred);
        if (inst == nullptr) {
          resourcePhi.addOperand(nullptr);
        } else if (auto value = inst.cast<ir::Value>()) {
          resourcePhi.addOperand(value);
        } else {
          auto block = resources.context.create<ir::Block>(
              inst.getLocation(), ir::Kind::Builtin, ir::builtin::BLOCK);
          inst.erase();
          block.addChild(inst);
          resourcePhi.addOperand(block);
        }
      }

      return resourcePhi;
    }

    return importIR(memorySSA, importer, def.getLinkedInst());
  }

  ir::Value toValue(ir::Instruction inst) {
    if (inst == nullptr) {
      return {};
    }

    if (auto value = inst.cast<ir::Value>()) {
      return value;
    }

    auto block = resources.context.create<ir::Block>(
        inst.getLocation(), ir::Kind::Builtin, ir::builtin::BLOCK);
    block.addChild(inst);
    return block;
  }

  ir::Instruction importIR(MemorySSA &memorySSA, spv::Import &importer,
                           ir::Instruction resource) {
    auto result = ir::clone(resource, resources.context, importer);
    std::vector<ir::Instruction> workList;
    workList.push_back(resource);
    std::set<ir::Instruction> visited;
    visited.insert(resource);

    while (!workList.empty()) {
      auto inst = workList.back();
      workList.pop_back();

      auto cloned = ir::clone(inst, resources.context, importer);

      if (inst == ir::spv::OpLoad) {
        auto def = memorySSA.getDef(inst, inst.getOperand(1).getAsValue());
        auto resourceInst = unpackResourceDef(memorySSA, importer, def);

        if (resourceInst == nullptr) {
          resources.hasUnknown = true;
          cloned.staticCast<ir::Value>().replaceAllUsesWith(nullptr);
          continue;
        }

        if (resourceInst != cloned) {
          if (resource == inst) {
            result = resourceInst;
          }

          cloned.staticCast<ir::Value>().replaceAllUsesWith(
              toValue(resourceInst));
          continue;
        }
      }

      if (inst == ir::spv::OpFunctionCall) {
        auto unpacked = unpackFunctionCall(memorySSA, importer,
                                           inst.staticCast<ir::Value>());

        if (unpacked) {
          cloned.staticCast<ir::Value>().replaceAllUsesWith(unpacked);
          if (resource == inst) {
            result = unpacked;
          }

          if (visited.insert(unpacked).second) {
            workList.emplace_back(unpacked);
          }

          continue;
        }
      }

      if (inst.getKind() != ir::Kind::Spv &&
          inst.getKind() != ir::Kind::AmdGpu) {
        auto unpacked = unpackInstruction(memorySSA, importer, inst);

        if (unpacked) {
          if (unpacked != inst) {
            cloned.staticCast<ir::Value>().replaceAllUsesWith(
                toValue(unpacked));

            if (resource == inst) {
              result = unpacked;
            }

            if (visited.insert(unpacked).second) {
              workList.emplace_back(unpacked);
            }

            continue;
          }

          // FIXME: pass read only parameters as value and remove this
          // workaround
          for (std::size_t i = 0, end = cloned.getOperandCount(); i < end;
               ++i) {
            auto valueOp = cloned.getOperand(i).getAsValue();
            if (valueOp == nullptr) {
              continue;
            }
            if (valueOp != ir::spv::OpVariable) {
              continue;
            }

            auto def = memorySSA.getDef(inst, inst.getOperand(i).getAsValue());
            if (auto resourceInst =
                    unpackResourceDef(memorySSA, importer, def)) {
              cloned.replaceOperand(i, toValue(resourceInst));
            }
          }

          continue;
        }
      }

      for (auto &operand : inst.getOperands()) {
        if (auto value = operand.getAsValue()) {
          if (value.getKind() == ir::Kind::Spv &&
              ir::spv::isTypeOp(value.getOp())) {
            continue;
          }

          if (visited.insert(value).second) {
            workList.emplace_back(value);
          }
        }
      }
    }

    return result;
  }

  int importResource(MemorySSA &memorySSA, spv::Import &resourceImporter,
                     ir::Instruction resource) {
    auto imported = importIR(memorySSA, resourceImporter, resource);

    std::vector<ir::Instruction> resourceSet{imported};

    int slot = -1;

    auto trackResource = [&](int resourceSlot) {
      if (slot == -1) {
        slot = resourceSlot;
      } else if (slot != resourceSlot) {
        slot = -2;
      }
    };

    for (auto inst : resourceSet) {
      if (inst == ir::amdgpu::POINTER) {
        std::uint32_t loadSize = *inst.getOperand(1).getAsInt32();
        auto base = inst.getOperand(2).getAsValue();
        auto offset = inst.getOperand(3).getAsValue();

        trackResource(addPointer({
            .size = loadSize,
            .base = base,
            .offset = offset,
        }));

        continue;
      }

      if (inst == ir::amdgpu::VBUFFER) {
        auto access = static_cast<Access>(*inst.getOperand(1).getAsInt32());
        auto words = inst.getOperands().subspan(2);

        trackResource(addBuffer({
            .access = access,
            .words = {words[0].getAsValue(), words[1].getAsValue(),
                      words[2].getAsValue(), words[3].getAsValue()},
        }));

        continue;
      }

      if (inst == ir::amdgpu::TBUFFER) {
        auto access = static_cast<Access>(*inst.getOperand(1).getAsInt32());
        auto words = inst.getOperands().subspan(2);
        if (words.size() > 4) {
          trackResource(addTexture({
              .access = access,
              .words = {words[0].getAsValue(), words[1].getAsValue(),
                        words[2].getAsValue(), words[3].getAsValue(),
                        words[4].getAsValue(), words[5].getAsValue(),
                        words[6].getAsValue(), words[7].getAsValue()},
          }));
        } else {
          trackResource(addTexture({
              .access = access,
              .words = {words[0].getAsValue(), words[1].getAsValue(),
                        words[2].getAsValue(), words[3].getAsValue()},
          }));
        }
        continue;
      }

      if (inst == ir::amdgpu::IMAGE_BUFFER) {
        auto access = static_cast<Access>(*inst.getOperand(1).getAsInt32());
        auto words = inst.getOperands().subspan(2);
        if (words.size() > 4) {
          trackResource(addImageBuffer({
              .access = access,
              .words = {words[0].getAsValue(), words[1].getAsValue(),
                        words[2].getAsValue(), words[3].getAsValue(),
                        words[4].getAsValue(), words[5].getAsValue(),
                        words[6].getAsValue(), words[7].getAsValue()},
          }));
        } else {
          trackResource(addImageBuffer({
              .access = access,
              .words = {words[0].getAsValue(), words[1].getAsValue(),
                        words[2].getAsValue(), words[3].getAsValue()},
          }));
        }
        continue;
      }

      if (inst == ir::amdgpu::SAMPLER) {
        auto words = inst.getOperands().subspan(1);
        auto unorm = *inst.getOperand(5).getAsBool();
        trackResource(addSampler({
            .unorm = unorm,
            .words = {words[0].getAsValue(), words[1].getAsValue(),
                      words[2].getAsValue(), words[3].getAsValue()},
        }));
        continue;
      }

      inst.print(std::cerr, *ns);
      rx::die("unexpected resource");
    }

    return slot;
  }
};

void gcn::Resources::print(std::ostream &os, ir::NameStorage &ns) const {
  os << "resource slots " << slots << ":\n";
  os << "has resources with unknown source: " << (hasUnknown ? "yes" : "no")
     << "\n";

  if (!pointers.empty()) {
    os << "pointers:\n";
    for (auto pointer : pointers) {
      os << " #" << pointer.resourceSlot << ":\n";
      os << "  base: ";
      printFlat(os, pointer.base, ns);
      os << "\n";
      os << "  offset: ";
      printFlat(os, pointer.offset, ns);
      os << "\n";
      os << "  size: " << pointer.size << "\n";
    }
  }

  auto printAccess = [&](Access access) {
    os << "  access: ";
    switch (access) {
    case Access::None:
      os << "none";
      break;
    case Access::Read:
      os << "read";
      break;
    case Access::Write:
      os << "write";
      break;
    case Access::ReadWrite:
      os << "read/write";
      break;
    default:
      os << "invalid";
      break;
    }
    os << "\n";
  };

  if (!textures.empty()) {
    os << "textures:\n";
    for (auto &texture : textures) {
      os << " #" << texture.resourceSlot << ":\n";
      printAccess(texture.access);

      for (auto &word : texture.words) {
        os << "  word" << (&word - texture.words) << ": ";
        printFlat(os, word, ns);
        os << "\n";
      }
    }
  }

  if (!imageBuffers.empty()) {
    os << "image buffers:\n";
    for (auto &buffer : buffers) {
      os << " #" << buffer.resourceSlot << ":\n";
      printAccess(buffer.access);

      for (auto &word : buffer.words) {
        os << "  word" << (&word - buffer.words) << ": ";
        printFlat(os, word, ns);
        os << "\n";
      }
    }
  }

  if (!buffers.empty()) {
    os << "buffers:\n";
    for (auto &buffer : buffers) {
      os << " #" << buffer.resourceSlot << ":\n";
      printAccess(buffer.access);

      for (auto &word : buffer.words) {
        os << "  word" << (&word - buffer.words) << ": ";
        printFlat(os, word, ns);
        os << "\n";
      }
    }
  }

  if (!samplers.empty()) {
    os << "samplers:\n";
    for (auto &sampler : samplers) {
      os << " #" << sampler.resourceSlot << ":\n";

      for (auto &word : sampler.words) {
        os << "  word" << (&word - sampler.words) << ": ";
        printFlat(os, word, ns);
        os << "\n";
      }
    }
  }
}

void gcn::Resources::dump() { print(std::cerr, context.ns); }

ir::Value GcnConverter::getGlPosition(gcn::Builder &builder) {
  auto float4OutPtrT = gcnContext.getTypePointer(
      ir::spv::StorageClass::Output,
      gcnContext.getTypeVector(gcnContext.getTypeFloat32(), 4));

  return builder.createSpvAccessChain(gcnContext.getUnknownLocation(),
                                      float4OutPtrT, gcnContext.perVertex,
                                      {{gcnContext.simm32(0)}});
}

ir::Value GcnConverter::getConfigBlock(int descriptorSet) {
  if (configBuffer != nullptr) {
    return configBuffer;
  }

  auto result = gcnContext.createRuntimeArrayUniformBuffer(
      descriptorSet, kConfigBinding, gcnContext.getTypeUInt32());
  auto blockStruct =
      result.getOperand(0).getAsValue().getOperand(1).getAsValue();

  gcnContext.setName(blockStruct, "Config");
  gcnContext.setName(result, "config");

  configBuffer = result;
  return result;
}

ir::Value GcnConverter::createReadConfig(gcn::Stage stage,
                                         gcn::Builder &builder,
                                         const ir::Operand &index) {
  auto userSgprsBlock = getConfigBlock(stageToDescriptorSet(stage));

  auto userSgprsPtrType = userSgprsBlock.getOperand(0).getAsValue();
  auto userSgprsStorageClass = static_cast<ir::spv::StorageClass>(
      *userSgprsPtrType.getOperand(0).getAsInt32());
  auto elemType = gcnContext.getTypeUInt32();

  auto elemPointer = gcnContext.getTypePointer(userSgprsStorageClass, elemType);
  auto loc = gcnContext.getUnknownLocation();

  auto ptr = builder.createSpvAccessChain(
      loc, elemPointer, userSgprsBlock,
      {{gcnContext.getIndex(0), gcnContext.getOperandValue(index)}});

  return builder.createSpvLoad(loc, elemType, ptr);
}

static int findArrayBounds(ir::Value variable) {
  int minReg = std::numeric_limits<int>::max();
  int maxReg = std::numeric_limits<int>::min();

  for (auto user : variable.getUserList()) {
    auto inst = user.cast<ir::Instruction>();
    if (inst == nullptr) {
      continue;
    }

    if (inst == ir::spv::OpAccessChain) {
      auto index = inst.getOperand(2).getAsValue();

      if (index != ir::spv::OpConstant) {
        return -1;
      }

      auto constIndex = index.getOperand(1).getAsInt32();

      if (constIndex == nullptr) {
        std::abort();
      }

      if (*constIndex > maxReg) {
        maxReg = *constIndex;
      }

      if (*constIndex < minReg) {
        minReg = *constIndex;
      }
    }
  }

  if (minReg > maxReg) {
    return 0;
  }

  return maxReg;
}

template <typename... IndiciesT>
ir::Value createPointerAccessChain(shader::spv::Context &context,
                                   ir::Location loc, gcn::Builder &builder,
                                   ir::Value type, ir::Value pointer,
                                   IndiciesT... indicies) {
  auto intT = context.getTypeSInt32();

  auto createIndex = [&](int index) {
    return context.getOrCreateConstant(intT, index);
  };

  auto pointerType = pointer.getOperand(0).getAsValue();
  if (pointerType != ir::spv::OpTypePointer) {
    std::abort();
  }

  auto storageClass = static_cast<ir::spv::StorageClass>(
      *pointerType.getOperand(0).getAsInt32());
  auto resultType = context.getTypePointer(storageClass, type);
  return builder.createSpvAccessChain(loc, resultType, pointer,
                                      {{createIndex(indicies)...}});
}

static void replaceVariableWithConstant(ir::Value variable,
                                        ir::Value constant) {
  while (!variable.getUseList().empty()) {
    auto use = *variable.getUseList().begin();

    if (use.user == ir::spv::OpName || use.user == ir::spv::OpDecorate) {
      use.user.remove();
      continue;
    }

    if (use.user == ir::spv::OpLoad) {
      use.user.staticCast<ir::Value>().replaceAllUsesWith(constant);
      use.user.remove();
      continue;
    }

    ir::NameStorage ns;
    use.user.print(std::cerr, ns);
    rx::die("replaceVariableWithConstant: unexpected variable user");
  }
}

static void expToSpv(GcnConverter &converter, gcn::Stage stage,
                     gcn::ShaderInfo &info, ir::Instruction inst) {
  enum Target : unsigned {
    ET_MRT0 = 0,
    ET_MRT7 = 7,
    ET_MRTZ = 8,
    ET_NULL = 9,
    ET_POS0 = 12,
    ET_POS3 = 15,
    ET_PARAM0 = 32,
    ET_PARAM31 = 63,
  };

  auto &context = converter.gcnContext;

  auto target = *inst.getOperand(0).getAsValue().getOperand(1).getAsInt32();
  auto swizzle = *inst.getOperand(1).getAsValue().getOperand(1).getAsInt32();
  auto comr = *inst.getOperand(2).getAsValue().getOperand(1).getAsInt32()
                  ? true
                  : false;
  auto done = *inst.getOperand(3).getAsValue().getOperand(1).getAsInt32()
                  ? true
                  : false;
  auto vm = *inst.getOperand(4).getAsValue().getOperand(1).getAsInt32() ? true
                                                                        : false;

  auto loc = inst.getLocation();
  auto builder = gcn::Builder::createInsertBefore(context, inst);

  auto cf0 = context.fimm32(0);
  auto elemType = context.getTypeFloat32();
  auto valueType = context.getTypeVector(elemType, 4);
  auto value = builder.createSpvCompositeConstruct(loc, valueType,
                                                   {{cf0, cf0, cf0, cf0}});

  if (comr) {
    int operandIndex = 5;
    for (auto channel = 0; channel < 2; ++channel) {
      if (~swizzle & (1 << (channel * 2))) {
        continue;
      }

      auto src = builder.createSpvBitcast(
          loc, context.getTypeFloat32(),
          inst.getOperand(operandIndex++).getAsValue());

      auto srcType = src.getOperand(0).getAsValue();
      ir::Value elementType;
      if (srcType == ir::spv::OpTypeFloat) {
        elementType = context.getTypeFloat16();
      } else if (srcType == ir::spv::OpTypeInt) {
        elementType =
            context.getTypeInt(16, *srcType.getOperand(1).getAsInt32() != 0);
      } else {
        std::abort();
      }

      auto elemVecT = context.getTypeVector(elementType, 2);
      src = builder.createSpvBitcast(loc, elemVecT, src);

      auto src0 =
          builder.createSpvCompositeExtract(loc, elementType, src, {{0}});
      auto src1 =
          builder.createSpvCompositeExtract(loc, elementType, src, {{1}});

      if (srcType == ir::spv::OpTypeFloat) {
        src0 = builder.createSpvFConvert(loc, context.getTypeFloat32(), src0);
        src1 = builder.createSpvFConvert(loc, context.getTypeFloat32(), src1);
      } else if (srcType == ir::spv::OpTypeInt) {
        if (*srcType.getOperand(1).getAsInt32() != 0) {
          src0 = builder.createSpvSConvert(loc, context.getTypeSInt32(), src0);
          src1 = builder.createSpvSConvert(loc, context.getTypeSInt32(), src1);
        } else {
          src0 = builder.createSpvUConvert(loc, context.getTypeUInt32(), src0);
          src1 = builder.createSpvUConvert(loc, context.getTypeUInt32(), src1);
        }
      } else {
        std::abort();
      }

      src0 = context.createCast(loc, builder, elemType, src0);
      src1 = context.createCast(loc, builder, elemType, src1);

      value = builder.createSpvCompositeInsert(loc, valueType, src0, value,
                                               {{channel * 2}});
      value = builder.createSpvCompositeInsert(loc, valueType, src1, value,
                                               {{channel * 2 + 1}});
    }
  } else {
    int operandIndex = 5;
    for (auto channel = 0; channel < 4; ++channel) {
      if (~swizzle & (1 << channel)) {
        continue;
      }

      value = builder.createSpvCompositeInsert(
          loc, valueType,
          context.createCast(loc, builder, elemType,
                             inst.getOperand(operandIndex++).getAsValue()),
          value, {{channel}});
    }
  }

  if (target == ET_POS0) {
    context.createPerVertex();
    auto glPosition = converter.getGlPosition(builder);
    auto channelType = context.getTypeFloat32();

    for (int channel = 0; channel < 4; ++channel) {
      if (~swizzle & (1 << channel)) {
        continue;
      }

      auto pointer = createPointerAccessChain(context, loc, builder,
                                              channelType, glPosition, channel);

      auto channelValue =
          builder.createSpvCompositeExtract(loc, elemType, value, {{channel}});

      channelValue =
          context.createCast(loc, builder, channelType, channelValue);

      if (channel < 3) {
        auto offsetId =
            gcn::ConfigType(int(gcn::ConfigType::ViewPortOffsetX) + channel);
        auto scaleId =
            gcn::ConfigType(int(gcn::ConfigType::ViewPortScaleX) + channel);
        auto offset = converter.createReadConfig(stage, builder,
                                                 info.create(offsetId, 0));
        auto scale =
            converter.createReadConfig(stage, builder, info.create(scaleId, 0));

        offset = builder.createSpvBitcast(loc, channelType, offset);
        scale = builder.createSpvBitcast(loc, channelType, scale);

        channelValue =
            builder.createSpvFMul(loc, channelType, channelValue, scale);
        channelValue =
            builder.createSpvFAdd(loc, channelType, channelValue, offset);
      }

      builder.createSpvStore(loc, pointer, channelValue);
    }

    return;
  }

  if (target >= ET_MRT0 && target <= ET_MRT7) {
    auto output = context.createOutput(loc, target - ET_MRT0);
    auto compSwap = converter.createReadConfig(
        stage, builder,
        info.create(gcn::ConfigType::CbCompSwap, target - ET_MRT0));

    value = builder.createValue(
        loc, ir::amdgpu::PS_COMP_SWAP, valueType,
        converter.createLocalVariable(builder, loc, compSwap),
        converter.createLocalVariable(builder, loc, value));

    auto channelType = context.getTypeFloat32();

    for (int channel = 0; channel < 4; ++channel) {
      if (~swizzle & (1 << channel)) {
        continue;
      }

      auto pointer = createPointerAccessChain(context, loc, builder,
                                              channelType, output, channel);
      auto channelValue =
          builder.createSpvCompositeExtract(loc, elemType, value, {{channel}});
      channelValue =
          context.createCast(loc, builder, channelType, channelValue);
      builder.createSpvStore(loc, pointer, channelValue);
    }

    return;
  }

  if (target == ET_MRTZ) {
    auto output = context.createFragDepth(loc);
    auto channelType = context.getTypeFloat32();

    for (int channel = 0; channel < 4; ++channel) {
      if (~swizzle & (1 << channel)) {
        continue;
      }

      auto channelValue =
          builder.createSpvCompositeExtract(loc, elemType, value, {{channel}});
      channelValue =
          context.createCast(loc, builder, channelType, channelValue);
      builder.createSpvStore(loc, output, channelValue);
      break;
    }

    return;
  }

  if (target >= ET_PARAM0 && target <= ET_PARAM31) {
    auto output = context.createOutput(loc, target - ET_PARAM0);
    auto floatT = context.getTypeFloat32();
    auto channelType = floatT;

    for (int channel = 0; channel < 4; ++channel) {
      if (~swizzle & (1 << channel)) {
        continue;
      }
      auto pointer = createPointerAccessChain(context, loc, builder,
                                              channelType, output, channel);
      auto channelValue =
          builder.createSpvCompositeExtract(loc, elemType, value, {{channel}});

      builder.createSpvStore(
          loc, pointer,
          context.createCast(loc, builder, channelType, channelValue));
    }

    return;
  }

  // FIXME

  auto targetToString = [](unsigned target) -> std::string {
    if (target >= ET_MRT0 && target <= ET_MRT7) {
      return "mrt" + std::to_string(target - ET_MRT0);
    }
    if (target == ET_MRTZ) {
      return "mrtz";
    }
    if (target == ET_NULL) {
      return "null";
    }
    if (target >= ET_POS0 && target <= ET_POS3) {
      return "pos" + std::to_string(target - ET_POS0);
    }
    if (target >= ET_PARAM0 && target <= ET_PARAM31) {
      return "param" + std::to_string(target - ET_PARAM0);
    }

    return std::to_string(target);
  };

  auto swizzleToString = [](unsigned swizzle) {
    std::string result;

    if (swizzle & 1) {
      result += 'x';
    }
    if (swizzle & 2) {
      result += 'y';
    }
    if (swizzle & 4) {
      result += 'z';
    }
    if (swizzle & 8) {
      result += 'w';
    }

    return result;
  };

  rx::println(stderr, "exp target {}.{}", targetToString(target),
              swizzleToString(swizzle));
  std::abort();
}

static void instructionsToSpv(GcnConverter &converter, gcn::Import &importer,
                              gcn::Stage stage, const gcn::Environment &env,
                              const SemanticInfo &semanticInfo,
                              const SemanticModuleInfo &semanticModuleInfo,
                              gcn::ShaderInfo &info, ir::Region body) {
  auto &context = converter.gcnContext;
  std::vector<ir::Value> toAnalyze;

  ir::Value baryCoordVar;
  ir::Value baryCoordNoPerspVar;
  auto glslStd450 =
      gcn::Builder::createAppend(
          context, context.layout.getOrCreateExtInstImports(context))
          .createSpvExtInstImport(context.getUnknownLocation(), "GLSL.std.450");
  auto boolT = context.getTypeBool();
  auto f32T = context.getTypeFloat32();
  auto s32T = context.getTypeSInt32();
  auto f32x3 = context.getTypeVector(f32T, 3);
  auto s32PT = context.getTypePointer(ir::spv::StorageClass::Input, s32T);
  auto f32x3PT = context.getTypePointer(ir::spv::StorageClass::Input, f32x3);

  auto f32x3array = context.getTypeArray(f32T, context.imm32(3));

  ir::Value sampleIdVar;

  if (env.supportsBarycentric && stage == gcn::Stage::Ps) {
    auto loc = context.getUnknownLocation();
    auto globals = gcn::Builder::createAppend(
        context, context.layout.getOrCreateGlobals(context));
    auto annotations = gcn::Builder::createAppend(
        context, context.layout.getOrCreateAnnotations(context));
    baryCoordVar =
        globals.createSpvVariable(loc, f32x3PT, ir::spv::StorageClass::Input);
    annotations.createSpvDecorate(
        loc, baryCoordVar,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::BaryCoordKHR));

    baryCoordNoPerspVar =
        globals.createSpvVariable(loc, f32x3PT, ir::spv::StorageClass::Input);
    annotations.createSpvDecorate(
        loc, baryCoordNoPerspVar,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::BaryCoordNoPerspKHR));

    sampleIdVar =
        globals.createSpvVariable(loc, s32PT, ir::spv::StorageClass::Input);
    annotations.createSpvDecorate(
        loc, sampleIdVar,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::SampleId));
    annotations.createSpvDecorate(loc, sampleIdVar,
                                  ir::spv::Decoration::Flat());
  }

  for (auto inst : body.children()) {
    if (inst == ir::exp::EXP) {
      expToSpv(converter, stage, info, inst);
      inst.remove();
      continue;
    }

    if (inst == ir::amdgpu::POINTER || inst == ir::amdgpu::VBUFFER ||
        inst == ir::amdgpu::SAMPLER || inst == ir::amdgpu::TBUFFER ||
        inst == ir::amdgpu::IMAGE_BUFFER) {
      toAnalyze.push_back(inst.staticCast<ir::Value>());
      continue;
    }

    if (inst.getKind() != ir::Kind::Spv && inst.getKind() != ir::Kind::AmdGpu &&
        semanticModuleInfo.findSemanticOf(inst.getInstId()) == nullptr) {
      std::println(stderr, "unimplemented semantic: ");
      inst.print(std::cerr, context.ns);
      std::println(stderr, "\n");

      std::vector<ir::Instruction> workList;
      std::set<ir::Instruction> removed;
      workList.push_back(inst);
      auto builder = gcn::Builder::createInsertBefore(context, inst);

      while (!workList.empty()) {
        auto inst = workList.back();
        workList.pop_back();

        if (!removed.insert(inst).second) {
          continue;
        }

        rx::println(stderr, "removing ");
        inst.print(std::cerr, context.ns);
        std::println(stderr, "\n");

        if (auto value = inst.cast<ir::Value>()) {
          for (auto &use : value.getUseList()) {
            if (removed.contains(use.user)) {
              continue;
            }

            workList.push_back(use.user);
          }

          value.replaceAllUsesWith(builder.createSpvUndef(
              inst.getLocation(), value.getOperand(0).getAsValue()));
        }

        inst.remove();
      }

      continue;
    }
  }

  if (!toAnalyze.empty()) {
    auto &cfg =
        context.analysis.get<CFG>([&] { return buildCFG(body.getFirst()); });

    ModuleInfo moduleInfo;
    collectModuleInfo(moduleInfo, context.layout);
    auto memorySSA = buildMemorySSA(
        cfg, semanticInfo,
        [&](int regId) {
          return context.getOrCreateRegisterVariable(gcn::RegId(regId));
        },
        &moduleInfo);

    spv::Import resourceImporter;
    // memorySSA.print(std::cerr, body, context.ns);

    ResourcesBuilder resourcesBuilder;
    std::map<ir::Value, std::int32_t> resourceConfigSlots;
    resourcesBuilder.ns = &context.ns;
    for (auto inst : toAnalyze) {
      std::uint32_t configSlot = -1;
      int resourceSlot =
          resourcesBuilder.importResource(memorySSA, resourceImporter, inst);
      if (resourceSlot >= 0) {
        configSlot = info.create(gcn::ConfigType::ResourceSlot, resourceSlot);
      }

      resourceConfigSlots[inst] = configSlot;
    }

    for (auto [inst, slot] : resourceConfigSlots) {
      auto builder = gcn::Builder::createInsertBefore(context, inst);
      if (slot >= 0) {
        auto value = converter.createReadConfig(stage, builder, slot);
        value = builder.createSpvBitcast(inst.getLocation(),
                                         context.getTypeSInt32(), value);
        inst.replaceAllUsesWith(value);
      } else {
        inst.replaceAllUsesWith(context.simm32(-1));
      }
      inst.remove();
    }

    info.resources = std::move(resourcesBuilder.resources);
  }

  for (auto inst : body.children()) {
    if (inst.getKind() == ir::Kind::Spv) {
      continue;
    }

    auto builder = gcn::Builder::createInsertBefore(context, inst);

    if (inst == ir::vintrp::P1_F32 || inst == ir::vintrp::P2_F32) {
      auto mode = builder.createSpvLoad(inst.getLocation(), f32T,
                                        inst.getOperand(2).getAsValue());
      mode = builder.createSpvBitcast(inst.getLocation(), s32T, mode);
      auto isPerspSample = builder.createSpvIEqual(
          inst.getLocation(), boolT, mode,
          context.imm32(static_cast<std::uint32_t>(
              inst == ir::vintrp::P1_F32 ? gcn::PsVGprInput::IPerspSample
                                         : gcn::PsVGprInput::JPerspSample)));
      auto isPerspCenter = builder.createSpvIEqual(
          inst.getLocation(), boolT, mode,
          context.imm32(static_cast<std::uint32_t>(
              inst == ir::vintrp::P1_F32 ? gcn::PsVGprInput::IPerspCenter
                                         : gcn::PsVGprInput::JPerspCenter)));
      auto isPerspCentroid = builder.createSpvIEqual(
          inst.getLocation(), boolT, mode,
          context.imm32(static_cast<std::uint32_t>(
              inst == ir::vintrp::P1_F32 ? gcn::PsVGprInput::IPerspCentroid
                                         : gcn::PsVGprInput::JPerspCentroid)));
      auto isLinearSample = builder.createSpvIEqual(
          inst.getLocation(), boolT, mode,
          context.imm32(static_cast<std::uint32_t>(
              inst == ir::vintrp::P1_F32 ? gcn::PsVGprInput::ILinearSample
                                         : gcn::PsVGprInput::JLinearSample)));
      auto isLinearCenter = builder.createSpvIEqual(
          inst.getLocation(), boolT, mode,
          context.imm32(static_cast<std::uint32_t>(
              inst == ir::vintrp::P1_F32 ? gcn::PsVGprInput::ILinearCenter
                                         : gcn::PsVGprInput::JLinearCenter)));
      auto isLinearCentroid = builder.createSpvIEqual(
          inst.getLocation(), boolT, mode,
          context.imm32(static_cast<std::uint32_t>(
              inst == ir::vintrp::P1_F32 ? gcn::PsVGprInput::ILinearCentroid
                                         : gcn::PsVGprInput::JLinearCentroid)));

      auto attr = inst.getOperand(3).getAsValue();

      if (env.supportsBarycentric) {
        attr = builder.createSpvLoad(inst.getLocation(), f32x3array, attr);
        auto sampleId =
            builder.createSpvLoad(inst.getLocation(), s32T, sampleIdVar);

        auto baryCoordPerspCenter =
            builder.createSpvLoad(inst.getLocation(), f32x3, baryCoordVar);
        auto baryCoordPerspSample = builder.createSpvExtInst(
            inst.getLocation(), f32x3, glslStd450,
            GLSLstd450InterpolateAtSample, {{baryCoordVar, sampleId}});
        auto baryCoordPerspCentroid = builder.createSpvExtInst(
            inst.getLocation(), f32x3, glslStd450,
            GLSLstd450InterpolateAtCentroid, {{baryCoordVar}});
        auto baryCoordLinearCenter = builder.createSpvLoad(
            inst.getLocation(), f32x3, baryCoordNoPerspVar);
        auto baryCoordLinearSample = builder.createSpvExtInst(
            inst.getLocation(), f32x3, glslStd450,
            GLSLstd450InterpolateAtSample, {{baryCoordNoPerspVar, sampleId}});
        auto baryCoordLinearCentroid = builder.createSpvExtInst(
            inst.getLocation(), f32x3, glslStd450,
            GLSLstd450InterpolateAtCentroid, {{baryCoordNoPerspVar}});

        ir::Value PerspSample;
        ir::Value PerspCenter;
        ir::Value PerspCentroid;
        ir::Value LinearSample;
        ir::Value LinearCenter;
        ir::Value LinearCentroid;

        if (inst == ir::vintrp::P1_F32) {
          auto attr0 = builder.createSpvCompositeExtract(inst.getLocation(),
                                                         f32T, attr, {{0}});
          auto attr1 = builder.createSpvCompositeExtract(inst.getLocation(),
                                                         f32T, attr, {{1}});
          auto baryCoordPerspCenterX = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspCenter, {{0}});
          auto baryCoordPerspCenterY = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspCenter, {{1}});
          auto baryCoordPerspSampleX = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspSample, {{0}});
          auto baryCoordPerspSampleY = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspSample, {{1}});
          auto baryCoordPerspCentroidX = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspCentroid, {{0}});
          auto baryCoordPerspCentroidY = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspCentroid, {{1}});
          auto baryCoordLinearCenterX = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearCenter, {{0}});
          auto baryCoordLinearCenterY = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearCenter, {{1}});
          auto baryCoordLinearSampleX = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearSample, {{0}});
          auto baryCoordLinearSampleY = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearSample, {{1}});
          auto baryCoordLinearCentroidX = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearCentroid, {{0}});
          auto baryCoordLinearCentroidY = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearCentroid, {{1}});

          auto PerspSample0 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordPerspSampleX, attr0);
          auto PerspSample1 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordPerspSampleY, attr1);
          auto PerspCenter0 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordPerspCenterX, attr0);
          auto PerspCenter1 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordPerspCenterY, attr1);
          auto PerspCentroid0 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordPerspCentroidX, attr0);
          auto PerspCentroid1 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordPerspCentroidY, attr1);
          auto LinearSample0 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordLinearSampleX, attr0);
          auto LinearSample1 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordLinearSampleY, attr1);
          auto LinearCenter0 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordLinearCenterX, attr0);
          auto LinearCenter1 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordLinearCenterY, attr1);
          auto LinearCentroid0 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordLinearCentroidX, attr0);
          auto LinearCentroid1 = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordLinearCentroidY, attr1);

          PerspSample = builder.createSpvFAdd(inst.getLocation(), f32T,
                                              PerspSample0, PerspSample1);
          PerspCenter = builder.createSpvFAdd(inst.getLocation(), f32T,
                                              PerspCenter0, PerspCenter1);
          PerspCentroid = builder.createSpvFAdd(inst.getLocation(), f32T,
                                                PerspCentroid0, PerspCentroid1);
          LinearSample = builder.createSpvFAdd(inst.getLocation(), f32T,
                                               LinearSample0, LinearSample1);
          LinearCenter = builder.createSpvFAdd(inst.getLocation(), f32T,
                                               LinearCenter0, LinearCenter1);
          LinearCentroid = builder.createSpvFAdd(
              inst.getLocation(), f32T, LinearCentroid0, LinearCentroid1);
        } else {
          auto dst = builder.createSpvLoad(inst.getLocation(), f32T,
                                           inst.getOperand(1).getAsValue());
          auto attr2 = builder.createSpvCompositeExtract(inst.getLocation(),
                                                         f32T, attr, {{2}});

          auto baryCoordPerspSampleZ = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspSample, {{2}});
          auto baryCoordPerspCenterZ = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspCenter, {{2}});
          auto baryCoordPerspCentroidZ = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordPerspCentroid, {{2}});
          auto baryCoordLinearSampleZ = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearSample, {{2}});
          auto baryCoordLinearCenterZ = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearCenter, {{2}});
          auto baryCoordLinearCentroidZ = builder.createSpvCompositeExtract(
              inst.getLocation(), f32T, baryCoordLinearCentroid, {{2}});

          PerspSample = builder.createSpvFMul(inst.getLocation(), f32T,
                                              baryCoordPerspSampleZ, attr2);
          PerspCenter = builder.createSpvFMul(inst.getLocation(), f32T,
                                              baryCoordPerspCenterZ, attr2);
          PerspCentroid = builder.createSpvFMul(inst.getLocation(), f32T,
                                                baryCoordPerspCentroidZ, attr2);
          LinearSample = builder.createSpvFMul(inst.getLocation(), f32T,
                                               baryCoordLinearSampleZ, attr2);
          LinearCenter = builder.createSpvFMul(inst.getLocation(), f32T,
                                               baryCoordLinearCenterZ, attr2);
          LinearCentroid = builder.createSpvFMul(
              inst.getLocation(), f32T, baryCoordLinearCentroidZ, attr2);

          PerspSample =
              builder.createSpvFAdd(inst.getLocation(), f32T, dst, PerspSample);
          PerspCenter =
              builder.createSpvFAdd(inst.getLocation(), f32T, dst, PerspCenter);
          PerspCentroid = builder.createSpvFAdd(inst.getLocation(), f32T, dst,
                                                PerspCentroid);
          LinearSample = builder.createSpvFAdd(inst.getLocation(), f32T, dst,
                                               LinearSample);
          LinearCenter = builder.createSpvFAdd(inst.getLocation(), f32T, dst,
                                               LinearCenter);
          LinearCentroid = builder.createSpvFAdd(inst.getLocation(), f32T, dst,
                                                 LinearCentroid);
        }

        attr = PerspCenter;
        attr = builder.createSpvSelect(inst.getLocation(), f32T, isPerspSample,
                                       PerspSample, attr);
        // attr = builder.createSpvSelect(inst.getLocation(), f32T,
        // isPerspCenter,
        //                                PerspCenter, attr);
        attr = builder.createSpvSelect(inst.getLocation(), f32T,
                                       isPerspCentroid, PerspCentroid, attr);
        attr = builder.createSpvSelect(inst.getLocation(), f32T, isLinearSample,
                                       LinearSample, attr);
        attr = builder.createSpvSelect(inst.getLocation(), f32T, isLinearCenter,
                                       LinearCenter, attr);
        attr = builder.createSpvSelect(inst.getLocation(), f32T,
                                       isLinearCentroid, LinearCentroid, attr);
      } else {
        attr = builder.createSpvLoad(inst.getLocation(), f32x3array, attr);
        attr = builder.createSpvCompositeExtract(inst.getLocation(), f32T, attr,
                                                 {{0}});
      }

      builder.createSpvStore(inst.getLocation(),
                             inst.getOperand(1).getAsValue(), attr);
      inst.remove();
      continue;
    }

    if (inst == ir::amdgpu::OMOD) {
      auto resultType = inst.getOperand(0).getAsValue();
      auto clamp = *inst.getOperand(1).getAsBool();
      auto omod = *inst.getOperand(2).getAsInt32();
      auto value = inst.getOperand(3).getAsValue();

      if (resultType == ir::spv::OpTypeInt) {
        auto floatType =
            context.getTypeFloat(*resultType.getOperand(0).getAsInt32());
        value = builder.createSpvBitcast(resultType.getLocation(), floatType,
                                         value);
        resultType = floatType;
      }

      if (resultType == ir::spv::OpTypeFloat) {
        auto resultWidth = *resultType.getOperand(0).getAsInt32();
        auto createConstant = [&](auto value) {
          return resultWidth == 64
                     ? context.getOrCreateConstant(resultType,
                                                   static_cast<double>(value))
                     : context.getOrCreateConstant(resultType,
                                                   static_cast<float>(value));
        };

        auto loc = inst.getLocation();

        switch (omod) {
        case 1:
          value =
              builder.createSpvFMul(loc, resultType, value, createConstant(2));
          break;

        case 2:
          value =
              builder.createSpvFMul(loc, resultType, value, createConstant(4));
          break;

        case 3:
          value =
              builder.createSpvFDiv(loc, resultType, value, createConstant(2));
          break;
        }

        if (clamp) {
          auto c0 = createConstant(0);
          auto c1 = createConstant(1);
          auto boolT = context.getTypeBool();

          value = builder.createSpvSelect(
              loc, resultType,
              builder.createSpvFOrdLessThan(loc, boolT, value, c0), c0, value);

          value = builder.createSpvSelect(
              loc, resultType,
              builder.createSpvFOrdGreaterThan(loc, boolT, value, c1), c1,
              value);
        }

        inst.staticCast<ir::Value>().replaceAllUsesWith(value);
      }

      inst.remove();
      continue;
    }

    if (inst == ir::amdgpu::NEG_ABS) {
      auto resultType = inst.getOperand(0).getAsValue();
      auto neg = *inst.getOperand(1).getAsBool();
      auto abs = *inst.getOperand(2).getAsBool();
      auto value = inst.getOperand(3).getAsValue();

      while (true) {
        auto valueType = value.getOperand(0).getAsValue();
        if (valueType == ir::spv::OpTypeFloat) {
          break;
        }

        if (value == ir::spv::OpBitcast) {
          value = value.getOperand(1).getAsValue();
          continue;
        }

        break;
      }

      auto loc = inst.getLocation();
      auto valueType = value.getOperand(0).getAsValue();

      if (valueType == ir::spv::OpTypeInt) {
        auto floatType =
            context.getTypeFloat(*valueType.getOperand(0).getAsInt32());
        value = builder.createSpvBitcast(loc, floatType, value);
        valueType = floatType;
      }
      auto width = *valueType.getOperand(0).getAsInt32();
      if (abs) {
        auto boolT = context.getTypeBool();
        auto c0 = width == 64 ? context.fimm64(0.0) : context.fimm32(0.0f);
        value = builder.createSpvSelect(
            loc, valueType,
            builder.createSpvFOrdLessThan(loc, boolT, value, c0),
            builder.createSpvFNegate(loc, valueType, value), value);
      }

      if (neg) {
        value = builder.createSpvFNegate(loc, valueType, value);
      }

      if (valueType != resultType) {
        value = builder.createSpvBitcast(loc, resultType, value);
      }

      inst.staticCast<ir::Value>().replaceAllUsesWith(value);
      inst.remove();
      continue;
    }

    auto function = semanticModuleInfo.findSemanticOf(inst.getInstId());

    if (function == nullptr) {
      continue;
    }

    function = ir::clone(function, context, importer);

    auto spvFnCall = builder.createSpvFunctionCall(
        inst.getLocation(), inst.getOperand(0).getAsValue(), function);

    for (auto &arg : inst.getOperands().subspan(1)) {
      spvFnCall.addOperand(arg);
    }

    if (auto val = inst.cast<ir::Value>()) {
      val.replaceAllUsesWith(spvFnCall);
    }

    inst.remove();
  }

  for (auto inst : body.children()) {
    if (inst.getKind() == ir::Kind::Spv) {
      continue;
    }

    auto builder = gcn::Builder::createInsertBefore(context, inst);

    if (inst == ir::amdgpu::IMM) {
      auto type = inst.getOperand(0).getAsValue();
      std::uint64_t address = *inst.getOperand(1).getAsInt64();
      std::uint32_t slot = info.create(gcn::ConfigType::Imm, address);

      auto materialized = converter.createReadConfig(stage, builder, slot);
      if (type != materialized.getOperand(0)) {
        materialized =
            builder.createSpvBitcast(inst.getLocation(), type, materialized);
      }
      inst.staticCast<ir::Value>().replaceAllUsesWith(materialized);
      inst.remove();
      continue;
    }

    if (inst == ir::amdgpu::USER_SGPR) {
      auto type = inst.getOperand(0).getAsValue();
      std::uint32_t index = *inst.getOperand(1).getAsInt32();
      std::uint32_t slot = info.create(gcn::ConfigType::UserSgpr, index);
      auto materialized = converter.createReadConfig(stage, builder, slot);
      if (type != materialized.getOperand(0)) {
        materialized =
            builder.createSpvBitcast(inst.getLocation(), type, materialized);
      }
      inst.staticCast<ir::Value>().replaceAllUsesWith(materialized);
      inst.remove();
      continue;
    }

    auto function = semanticModuleInfo.findSemanticOf(inst.getInstId());

    function = ir::clone(function, context, importer);

    auto spvFnCall = builder.createSpvFunctionCall(
        inst.getLocation(), inst.getOperand(0).getAsValue(), function);

    for (auto arg : inst.getOperands().subspan(1)) {
      spvFnCall.addOperand(arg);
    }

    if (auto val = inst.cast<ir::Value>()) {
      val.replaceAllUsesWith(spvFnCall);
    }

    inst.remove();
  }
}

static void createEntryPoint(gcn::Context &context, const gcn::Environment &env,
                             gcn::Stage stage, ir::Region &&body) {
  auto executionModel = ir::spv::ExecutionModel::GLCompute;

  switch (stage) {
  case gcn::Stage::Ps:
    executionModel = ir::spv::ExecutionModel::Fragment;
    break;
  case gcn::Stage::Gs:
    executionModel = ir::spv::ExecutionModel::Geometry;
    break;
  case gcn::Stage::DsVs:
  case gcn::Stage::VsVs:
  case gcn::Stage::GsVs:
    executionModel = ir::spv::ExecutionModel::Vertex;
    break;

  case gcn::Stage::VsEs:
  case gcn::Stage::DsEs:
  case gcn::Stage::VsLs:
    executionModel = ir::spv::ExecutionModel::TessellationEvaluation;
    break;

  case gcn::Stage::Hs:
    executionModel = ir::spv::ExecutionModel::TessellationControl;
    break;

  case gcn::Stage::Cs:
    executionModel = ir::spv::ExecutionModel::GLCompute;
    break;
  case gcn::Stage::Invalid:
    rx::die("invalid shader stage");
  }

  std::vector<ir::spv::IdRef> interfaceList;

  for (auto global :
       context.layout.getOrCreateGlobals(context).children<ir::Value>()) {
    if (global == ir::spv::OpVariable) {
      interfaceList.push_back(global);
    }
  }

  auto mainLoc = context.getUnknownLocation();
  auto prologueBlock = context.createRegionWithLabel(mainLoc);
  auto prologue = gcn::Builder::createInsertBefore(context, prologueBlock);
  auto mainReturnT = context.getTypeVoid();
  auto mainFnT = context.getTypeFunction(context.getTypeVoid(), {});

  auto mainFn = prologue.createSpvFunction(
      mainLoc, mainReturnT, ir::spv::FunctionControl::None, mainFnT);

  gcn::Builder::createAppend(context, context.localVariables)
      .createSpvBranch(context.getUnknownLocation(), context.entryPoint);

  prologueBlock.getParent().appendRegion(context.localVariables);

  auto epilogue = gcn::Builder::createAppend(context, context.epilogue);
  epilogue.createSpvReturn(mainLoc);
  epilogue.createSpvFunctionEnd(mainLoc);

  auto functions = context.layout.getOrCreateFunctions(context);
  functions.appendRegion(prologueBlock.getParent());

  for (auto cfg = buildCFG(body.getFirst()); auto bb : cfg.getPreorderNodes()) {
    for (auto child : bb->range()) {
      child.erase();
      functions.addChild(child);
    }
  }

  functions.appendRegion(context.epilogue);

  auto entryPoints = gcn::Builder::createAppend(
      context, context.layout.getOrCreateEntryPoints(context));

  if (executionModel == ir::spv::ExecutionModel::Fragment) {
    auto executionModes = gcn::Builder::createAppend(
        context, context.layout.getOrCreateExecutionModes(context));

    executionModes.createSpvExecutionMode(
        mainFn.getLocation(), mainFn,
        ir::spv::ExecutionMode::OriginUpperLeft());
    executionModes.createSpvExecutionMode(
        mainFn.getLocation(), mainFn, ir::spv::ExecutionMode::DepthReplacing());
  }

  if (executionModel == ir::spv::ExecutionModel::GLCompute) {
    auto executionModes = gcn::Builder::createAppend(
        context, context.layout.getOrCreateExecutionModes(context));

    executionModes.createSpvExecutionMode(
        mainFn.getLocation(), mainFn,
        ir::spv::ExecutionMode::LocalSize(env.numThreadX, env.numThreadY,
                                          env.numThreadZ));
  }

  entryPoints.createSpvEntryPoint(mainFn.getLocation(), executionModel, mainFn,
                                  "main", interfaceList);
}

static void createInitialValues(GcnConverter &converter,
                                const gcn::Environment &env, gcn::Stage stage,
                                gcn::ShaderInfo &info, ir::Region body) {
  auto &context = converter.gcnContext;
  auto builder = gcn::Builder::createInsertAfter(context, body.getFirst());

  auto loc = context.getUnknownLocation();

  if (stage != gcn::Stage::Cs) {
    context.writeReg(loc, builder, gcn::RegId::Exec, 0, context.imm64(1));
  }

  if (stage == gcn::Stage::VsVs || stage == gcn::Stage::GsVs ||
      stage == gcn::Stage::DsVs) {
    auto inputType = context.getTypePointer(ir::spv::StorageClass::Input,
                                            context.getTypeUInt32());

    auto globals = gcn::Builder::createAppend(
        context, context.layout.getOrCreateGlobals(context));
    auto annotations = gcn::Builder::createAppend(
        context, context.layout.getOrCreateAnnotations(context));

    auto vertexIndexVariable =
        globals.createSpvVariable(loc, inputType, ir::spv::StorageClass::Input);

    annotations.createSpvDecorate(
        loc, vertexIndexVariable,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::VertexIndex));

    auto vertexIndex = builder.createSpvLoad(loc, context.getTypeUInt32(),
                                             vertexIndexVariable);

    auto primType = converter.createReadConfig(
        stage, builder, info.create(gcn::ConfigType::VsPrimType, 0));
    auto indexOffset = converter.createReadConfig(
        stage, builder, info.create(gcn::ConfigType::VsIndexOffset, 0));
    primType = converter.createLocalVariable(builder, loc, primType);
    vertexIndex = converter.createLocalVariable(builder, loc, vertexIndex);
    indexOffset = converter.createLocalVariable(builder, loc, indexOffset);

    vertexIndex = builder.createValue(
        loc, ir::amdgpu::VS_GET_INDEX,
        {{context.getTypeUInt32(), primType, vertexIndex, indexOffset}});

    context.writeReg(loc, builder, gcn::RegId::Vgpr, 0, vertexIndex);
  } else if (stage == gcn::Stage::Ps) {
    auto boolT = context.getTypeBool();
    auto f32T = context.getTypeFloat32();
    auto f32x4 = context.getTypeVector(f32T, 4);

    auto boolPT = context.getTypePointer(ir::spv::StorageClass::Input, boolT);
    auto f32x4PT = context.getTypePointer(ir::spv::StorageClass::Input, f32x4);

    auto globals = gcn::Builder::createAppend(
        context, context.layout.getOrCreateGlobals(context));
    auto annotations = gcn::Builder::createAppend(
        context, context.layout.getOrCreateAnnotations(context));
    auto capabilities = gcn::Builder::createAppend(
        context, context.layout.getOrCreateCapabilities(context));
    auto extensions = gcn::Builder::createAppend(
        context, context.layout.getOrCreateExtensions(context));

    if (env.supportsBarycentric) {
      capabilities.createSpvCapability(
          loc, ir::spv::Capability::FragmentBarycentricKHR);
      extensions.createSpvExtension(loc, "SPV_KHR_fragment_shader_barycentric");
    }
    capabilities.createSpvCapability(
        loc, ir::spv::Capability::InterpolationFunction);
    capabilities.createSpvCapability(loc,
                                     ir::spv::Capability::SampleRateShading);

    auto fragCoordVar =
        globals.createSpvVariable(loc, f32x4PT, ir::spv::StorageClass::Input);
    annotations.createSpvDecorate(
        loc, fragCoordVar,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::FragCoord));

    auto frontFaceVar =
        globals.createSpvVariable(loc, boolPT, ir::spv::StorageClass::Input);
    annotations.createSpvDecorate(
        loc, frontFaceVar,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::FrontFacing));

    auto fragCoord = builder.createSpvLoad(loc, f32x4, fragCoordVar);
    auto frontFace = builder.createSpvLoad(loc, boolT, frontFaceVar);
    auto indexLocal =
        converter.createLocalVariable(builder, loc, context.simm32(0));

    fragCoord = converter.createLocalVariable(builder, loc, fragCoord);
    frontFace = converter.createLocalVariable(builder, loc, frontFace);

    for (int i = 0;
         i < std::min<int>(env.vgprCount,
                           static_cast<int>(gcn::PsVGprInput::Count));
         ++i) {
      std::uint32_t slot = info.create(gcn::ConfigType::PsInputVGpr, i);
      auto runtimeIndex = converter.createReadConfig(stage, builder, slot);

      builder.createSpvStore(
          loc, indexLocal,
          builder.createSpvBitcast(loc, context.getTypeSInt32(), runtimeIndex));

      auto vgprValue = builder.createValue(loc, ir::amdgpu::PS_INPUT_VGPR,
                                           context.getTypeFloat32(), indexLocal,
                                           fragCoord, frontFace);
      context.writeReg(loc, builder, gcn::RegId::Vgpr, i, vgprValue);
    }
  }

  if (stage == gcn::Stage::Cs) {
    auto uintT = context.getTypeUInt32();
    auto uvec3T = context.getTypeVector(uintT, 3);
    auto pInputUVec3T =
        context.getTypePointer(ir::spv::StorageClass::Input, uvec3T);

    auto globals = gcn::Builder::createAppend(
        context, context.layout.getOrCreateGlobals(context));
    auto annotations = gcn::Builder::createAppend(
        context, context.layout.getOrCreateAnnotations(context));

    auto workGroupIdVar = globals.createSpvVariable(
        loc, pInputUVec3T, ir::spv::StorageClass::Input);
    annotations.createSpvDecorate(
        loc, workGroupIdVar,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::WorkgroupId));

    auto localInvocationIdVar = globals.createSpvVariable(
        loc, pInputUVec3T, ir::spv::StorageClass::Input);
    annotations.createSpvDecorate(
        loc, localInvocationIdVar,
        ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::LocalInvocationId));

    auto workGroupId = builder.createSpvLoad(loc, uvec3T, workGroupIdVar);
    auto workGroupIdLocalVar =
        converter.createLocalVariable(builder, loc, workGroupId);
    auto localInvocationId =
        builder.createSpvLoad(loc, uvec3T, localInvocationIdVar);
    auto localInvocationIdLocVar =
        converter.createLocalVariable(builder, loc, localInvocationId);

    {
      auto indexLocal =
          converter.createLocalVariable(builder, loc, context.simm32(0));
      int end = env.sgprCount;
      end = std::min<int>(end, env.userSgprs.size() +
                                   static_cast<int>(gcn::CsSGprInput::Count));

      for (int i = env.userSgprs.size(); i < end; ++i) {
        std::uint32_t slot =
            info.create(gcn::ConfigType::CsInputSGpr, i - env.userSgprs.size());
        auto runtimeIndex = converter.createReadConfig(stage, builder, slot);
        builder.createSpvStore(loc, indexLocal,
                               builder.createSpvBitcast(
                                   loc, context.getTypeSInt32(), runtimeIndex));

        auto sgprValue = builder.createValue(loc, ir::amdgpu::CS_INPUT_SGPR,
                                             context.getTypeUInt32(),
                                             indexLocal, workGroupIdLocalVar);
        context.writeReg(loc, builder, gcn::RegId::Sgpr, i, sgprValue);
      }
    }

    auto workgroupSize = builder.createSpvCompositeConstruct(
        loc, uvec3T,
        {{context.imm32(env.numThreadX), context.imm32(env.numThreadY),
          context.imm32(env.numThreadZ)}});
    auto workgroupSizeLocVar =
        converter.createLocalVariable(builder, loc, workgroupSize);

    builder.createValue(loc, ir::amdgpu::CS_SET_THREAD_ID,
                        context.getTypeVoid(), localInvocationIdLocVar,
                        workgroupSizeLocVar);

    builder.createValue(loc, ir::amdgpu::CS_SET_INITIAL_EXEC,
                        context.getTypeVoid(), localInvocationIdLocVar,
                        workgroupSizeLocVar);

    for (std::int32_t i = 0; i < 3; ++i) {
      auto value = builder.createSpvCompositeExtract(loc, uintT,
                                                     localInvocationId, {{i}});
      context.writeReg(loc, builder, gcn::RegId::Vgpr, i, value);
    }
  }

  context.writeReg(loc, builder, gcn::RegId::Vcc, 0, context.imm64(0));

  for (int word = 0; word < 2; ++word) {
    context.writeReg(
        loc, builder, gcn::RegId::MemoryTable, word,
        converter.createReadConfig(
            stage, builder, info.create(gcn::ConfigType::MemoryTable, word)));
  }

  for (int word = 0; word < 2; ++word) {
    context.writeReg(loc, builder, gcn::RegId::ImageMemoryTable, word,
                     converter.createReadConfig(
                         stage, builder,
                         info.create(gcn::ConfigType::ImageMemoryTable, word)));
  }

  for (int word = 0; word < 2; ++word) {
    context.writeReg(
        loc, builder, gcn::RegId::Gds, word,
        converter.createReadConfig(stage, builder,
                                   info.create(gcn::ConfigType::Gds, word)));
  }
}

std::optional<gcn::ConvertedShader>
gcn::convertToSpv(Context &context, ir::Region body,
                  const SemanticInfo &semanticInfo,
                  const SemanticModuleInfo &semanticModule, Stage stage,
                  const Environment &env) {
  gcn::ConvertedShader result;
  GcnConverter converter{context};
  gcn::Import importer;

  createInitialValues(converter, env, stage, result.info, body);
  instructionsToSpv(converter, importer, stage, env, semanticInfo,
                    semanticModule, result.info, body);
  if (stage != gcn::Stage::Cs) {
    replaceVariableWithConstant(
        context.getOrCreateRegisterVariable(gcn::RegId::ThreadId),
        context.imm32(0));
  }

  createEntryPoint(context, env, stage, std::move(body));

  for (int userSgpr = std::countr_zero(context.requiredUserSgprs);
       userSgpr < 32;
       userSgpr +=
       std::countr_zero(context.requiredUserSgprs >> (userSgpr + 1)) + 1) {
    result.info.requiredSgprs.push_back({userSgpr, env.userSgprs[userSgpr]});
  }

  auto memModel = Builder::createAppend(
      context, context.layout.getOrCreateMemoryModels(context));
  auto capabilities = Builder::createAppend(
      context, context.layout.getOrCreateCapabilities(context));
  auto extensions = gcn::Builder::createAppend(
      context, context.layout.getOrCreateExtensions(context));

  memModel.createSpvMemoryModel(
      context.getUnknownLocation(),
      ir::spv::AddressingModel::PhysicalStorageBuffer64,
      ir::spv::MemoryModel::GLSL450);

  for (auto cap : {
           ir::spv::Capability::Shader,
           ir::spv::Capability::Float16,
           ir::spv::Capability::Float64,
           ir::spv::Capability::Int64,
           ir::spv::Capability::Int16,
           ir::spv::Capability::StorageBuffer16BitAccess,
           ir::spv::Capability::PhysicalStorageBufferAddresses,
           ir::spv::Capability::Sampled1D,
           ir::spv::Capability::Image1D,
           ir::spv::Capability::RuntimeDescriptorArrayEXT,
       }) {
    capabilities.createSpvCapability(context.getUnknownLocation(), cap);
  }

  extensions.createSpvExtension(context.getUnknownLocation(),
                                "SPV_EXT_descriptor_indexing");

  if (env.supportsInt8) {
    for (auto cap : {
             ir::spv::Capability::Int8,
             ir::spv::Capability::StorageBuffer8BitAccess,
         }) {
      capabilities.createSpvCapability(context.getUnknownLocation(), cap);
    }

    extensions.createSpvExtension(context.getUnknownLocation(),
                                  "SPV_KHR_8bit_storage");
  }

  if (env.supportsInt64Atomics) {
    capabilities.createSpvCapability(context.getUnknownLocation(),
                                     ir::spv::Capability::Int64Atomics);
  }

  capabilities.createSpvCapability(context.getUnknownLocation(),
                                   ir::spv::Capability::ImageQuery);

  extensions.createSpvExtension(context.getUnknownLocation(),
                                "SPV_KHR_physical_storage_buffer");

  if (env.supportsNonSemanticInfo) {
    extensions.createSpvExtension(context.getUnknownLocation(),
                                  "SPV_KHR_non_semantic_info");
  } else {
    for (auto imported : context.layout.getOrCreateExtInstImports(context)
                             .children<ir::Value>()) {
      if (imported.getOperand(0) == "NonSemantic.DebugPrintf") {
        while (!imported.getUseList().empty()) {
          auto use = *imported.getUseList().begin();
          use.user.remove();
        }

        imported.remove();
      }
    }
  }

  auto merged = context.layout.merge(context);
  result.spv = spv::serialize(merged);
  result.info.memoryMap = std::move(context.memoryMap);
  return result;
}
