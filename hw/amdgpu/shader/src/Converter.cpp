#include "Converter.hpp"
#include "CfBuilder.hpp"
#include "ConverterContext.hpp"
#include "Fragment.hpp"
#include "FragmentTerminator.hpp"
#include "Instruction.hpp"
#include "RegisterId.hpp"
#include "RegisterState.hpp"
#include "cf.hpp"
#include "amdgpu/RemoteMemory.hpp"
#include "scf.hpp"
#include "util/unreachable.hpp"
#include <compare>
#include <cstddef>
#include <forward_list>
#include <memory>
#include <spirv/spirv.hpp>
#include <unordered_map>
#include <utility>
#include <vector>

static void printInstructions(const scf::PrintOptions &options, unsigned depth,
                              std::uint32_t *instBegin, std::size_t size) {
  auto instHex = instBegin;
  auto instEnd = instBegin + size / sizeof(std::uint32_t);

  while (instHex < instEnd) {
    auto instruction = amdgpu::shader::Instruction(instHex);
    std::printf("%s", options.makeIdent(depth).c_str());
    instruction.dump();
    std::printf("\n");
    instHex += instruction.size();
  }
}

namespace amdgpu::shader {
class Converter {
  scf::Context *scfContext;
  cf::Context cfContext;
  RemoteMemory memory;
  Function *function = nullptr;
  std::forward_list<RegisterState> states;
  std::vector<RegisterState *> freeStates;

public:
  void convertFunction(RemoteMemory mem, scf::Context *scfCtxt,
                       scf::Block *block, Function *fn) {
    scfContext = scfCtxt;
    function = fn;
    memory = mem;

    auto lastFragment = convertBlock(block, &function->entryFragment);

    if (lastFragment != nullptr) {
      lastFragment->builder.createBranch(fn->exitFragment.entryBlockId);
      lastFragment->appendBranch(fn->exitFragment);
    }

    initState(&fn->exitFragment);
  }

private:
  RegisterState *allocateState() {
    if (freeStates.empty()) {
      return &states.emplace_front();
    }

    auto result = freeStates.back();
    freeStates.pop_back();
    *result = {};
    return result;
  }

  void releaseState(RegisterState *state) {
    assert(state != nullptr);
    freeStates.push_back(state);
  }

  void initState(Fragment *fragment, std::uint64_t address = 0) {
    if (fragment->registers == nullptr) {
      fragment->registers = allocateState();
    }

    if (address != 0) {
      fragment->registers->pc = address;
    }

    fragment->injectValuesFromPreds();
    fragment->predecessors.clear();
  }

  void releaseStateOf(Fragment *frag) {
    releaseState(frag->registers);
    frag->registers = nullptr;
    frag->values = {};
    frag->outputs = {};
  }

  bool needInjectExecTest(Fragment *fragment) {
    auto inst = memory.getPointer<std::uint32_t>(fragment->registers->pc);
    auto instClass = getInstructionClass(*inst);
    return instClass == InstructionClass::Vop2 ||
           instClass == InstructionClass::Vop3 ||
           instClass == InstructionClass::Mubuf ||
           instClass == InstructionClass::Mtbuf ||
           instClass == InstructionClass::Mimg ||
           instClass == InstructionClass::Ds ||
           instClass == InstructionClass::Vintrp ||
           instClass == InstructionClass::Exp ||
           instClass == InstructionClass::Vop1 ||
           instClass == InstructionClass::Vopc/* ||
           instClass == InstructionClass::Smrd*/;
  }

  spirv::BoolValue createExecTest(Fragment *fragment) {
    auto context = fragment->context;
    auto &builder = fragment->builder;
    auto boolT = context->getBoolType();
    auto uint32_0 = context->getUInt32(0);
    auto loIsNotZero =
        builder.createINotEqual(boolT, fragment->getExecLo().value, uint32_0);
    auto hiIsNotZero =
        builder.createINotEqual(boolT, fragment->getExecHi().value, uint32_0);

    return builder.createLogicalOr(boolT, loIsNotZero, hiIsNotZero);
  }

  Fragment *convertBlock(scf::Block *block, Fragment *rootFragment) {
    Fragment *currentFragment = nullptr;

    for (scf::Node *node = block->getRootNode(); node != nullptr;
         node = node->getNext()) {

      if (auto bb = dynCast<scf::BasicBlock>(node)) {
        if (currentFragment == nullptr) {
          currentFragment = rootFragment;
        } else {
          auto newFragment = function->createFragment();
          currentFragment->appendBranch(*newFragment);
          currentFragment->builder.createBranch(newFragment->entryBlockId);
          currentFragment = newFragment;
        }

        initState(currentFragment, bb->getAddress());
        for (auto pred : currentFragment->predecessors) {
          releaseStateOf(pred);
        }

        if (needInjectExecTest(currentFragment)) {
          auto bodyFragment = function->createFragment();
          auto mergeFragment = function->createFragment();

          auto cond = createExecTest(currentFragment);

          currentFragment->appendBranch(*bodyFragment);
          currentFragment->appendBranch(*mergeFragment);
          currentFragment->builder.createSelectionMerge(
              mergeFragment->entryBlockId, {});
          currentFragment->builder.createBranchConditional(
              cond, bodyFragment->entryBlockId, mergeFragment->entryBlockId);

          initState(bodyFragment, bb->getAddress());
          bodyFragment->convert(bb->getSize());

          bodyFragment->appendBranch(*mergeFragment);
          bodyFragment->builder.createBranch(mergeFragment->entryBlockId);

          initState(mergeFragment);
          releaseState(currentFragment->registers);
          releaseState(bodyFragment->registers);

          currentFragment = mergeFragment;
        } else {
          currentFragment->convert(bb->getSize());
        }
        continue;
      }

      if (auto ifElse = dynCast<scf::IfElse>(node)) {
        auto ifTrueFragment = function->createFragment();
        auto ifFalseFragment = function->createFragment();
        auto mergeFragment = function->createFragment();

        currentFragment->appendBranch(*ifTrueFragment);
        currentFragment->appendBranch(*ifFalseFragment);

        currentFragment->builder.createSelectionMerge(
            mergeFragment->entryBlockId, {});
        currentFragment->builder.createBranchConditional(
            currentFragment->branchCondition, ifTrueFragment->entryBlockId,
            ifFalseFragment->entryBlockId);

        auto ifTrueLastBlock = convertBlock(ifElse->ifTrue, ifTrueFragment);
        auto ifFalseLastBlock = convertBlock(ifElse->ifFalse, ifFalseFragment);

        if (ifTrueLastBlock != nullptr) {
          ifTrueLastBlock->builder.createBranch(mergeFragment->entryBlockId);
          ifTrueLastBlock->appendBranch(*mergeFragment);

          if (ifTrueLastBlock->registers == nullptr) {
            initState(ifTrueLastBlock);
          }
        }

        if (ifFalseLastBlock != nullptr) {
          ifFalseLastBlock->builder.createBranch(mergeFragment->entryBlockId);
          ifFalseLastBlock->appendBranch(*mergeFragment);

          if (ifFalseLastBlock->registers == nullptr) {
            initState(ifFalseLastBlock);
          }
        }

        releaseStateOf(currentFragment);
        initState(mergeFragment);

        if (ifTrueLastBlock != nullptr) {
          releaseStateOf(ifTrueLastBlock);
        }

        if (ifFalseLastBlock != nullptr) {
          releaseStateOf(ifFalseLastBlock);
        }
        currentFragment = mergeFragment;
        continue;
      }

      if (dynCast<scf::UnknownBlock>(node)) {
        auto jumpAddress = currentFragment->jumpAddress;

        std::printf("jump to %lx\n", jumpAddress);
        std::fflush(stdout);

        if (jumpAddress == 0) {
          util::unreachable("no jump register on unknown block");
        }

        auto block = buildCf(cfContext, memory, jumpAddress);
        auto basicBlockPrinter = [this](const scf::PrintOptions &opts,
                                        unsigned depth, scf::BasicBlock *bb) {
          printInstructions(opts, depth,
                            memory.getPointer<std::uint32_t>(bb->getAddress()),
                            bb->getSize());
        };
        auto scfBlock = scf::structurize(*scfContext, block);
        scfBlock->print({.blockPrinter = basicBlockPrinter}, 0);
        std::fflush(stdout);

        auto targetFragment = function->createFragment();
        currentFragment->builder.createBranch(targetFragment->entryBlockId);
        currentFragment->appendBranch(*targetFragment);
        auto result = convertBlock(scfBlock, targetFragment);

        if (currentFragment->registers == nullptr) {
          initState(targetFragment);
          releaseStateOf(currentFragment);
        }

        return result;
      }

      if (dynCast<scf::Return>(node)) {
        currentFragment->appendBranch(function->exitFragment);
        currentFragment->builder.createBranch(
            function->exitFragment.entryBlockId);
        return nullptr;
      }

      util::unreachable();
    }

    return currentFragment != nullptr ? currentFragment : rootFragment;
  }
};
}; // namespace amdgpu::shader

amdgpu::shader::Shader amdgpu::shader::convert(
    RemoteMemory memory, Stage stage, std::uint64_t entry,
    std::span<const std::uint32_t> userSpgrs, int bindingOffset,
    std::uint32_t dimX, std::uint32_t dimY, std::uint32_t dimZ) {
  ConverterContext ctxt(memory, stage);
  auto &builder = ctxt.getBuilder();
  builder.createCapability(spv::Capability::Shader);
  builder.createCapability(spv::Capability::ImageQuery);
  builder.createCapability(spv::Capability::ImageBuffer);
  builder.createCapability(spv::Capability::UniformAndStorageBuffer8BitAccess);
  builder.createCapability(spv::Capability::UniformAndStorageBuffer16BitAccess);
  builder.createCapability(spv::Capability::Int64);
  builder.setMemoryModel(spv::AddressingModel::Logical,
                         spv::MemoryModel::GLSL450);

  scf::Context scfContext;
  scf::Block *entryBlock = nullptr;
  {
    cf::Context cfContext;
    auto entryBB = buildCf(cfContext, memory, entry);
    entryBlock = scf::structurize(scfContext, entryBB);
  }

  // std::printf("========== stage: %u, user sgprs: %zu\n", (unsigned)stage,
  //             userSpgrs.size());
  // std::printf("structurized CFG:\n");

  // auto basicBlockPrinter = [memory](const scf::PrintOptions &opts,
  //                                   unsigned depth, scf::BasicBlock *bb) {
  //   printInstructions(opts, depth,
  //                     memory.getPointer<std::uint32_t>(bb->getAddress()),
  //                     bb->getSize());
  // };

  // entryBlock->print({.blockPrinter = basicBlockPrinter}, 0);
  // std::printf("==========\n");

  auto mainFunction = ctxt.createFunction(0);
  mainFunction->userSgprs = userSpgrs;
  mainFunction->stage = stage;

  Converter converter;
  converter.convertFunction(memory, &scfContext, entryBlock, mainFunction);

  Shader result;

  std::fflush(stdout);
  mainFunction->exitFragment.outputs.clear();

  for (auto &uniform : ctxt.getUniforms()) {
    auto &newUniform = result.uniforms.emplace_back();
    newUniform.binding = bindingOffset++;

    for (int i = 0; i < 8; ++i) {
      newUniform.buffer[i] = uniform.buffer[i];
    }

    std::uint32_t descriptorSet = 0;

    ctxt.getBuilder().createDecorate(
        uniform.variable, spv::Decoration::DescriptorSet, {{descriptorSet}});
    ctxt.getBuilder().createDecorate(uniform.variable, spv::Decoration::Binding,
                                     {{newUniform.binding}});

    switch (uniform.typeId) {
    case TypeId::Sampler:
      newUniform.kind = Shader::UniformKind::Sampler;
      break;
    case TypeId::Image2D:
      newUniform.kind = Shader::UniformKind::Image;
      break;
    default:
      newUniform.kind = Shader::UniformKind::Buffer;
      break;
    }

    newUniform.accessOp = uniform.accessOp;
  }

  mainFunction->insertReturn();

  for (auto frag : mainFunction->fragments) {
    mainFunction->builder.insertBlock(frag->builder);
  }

  mainFunction->builder.insertBlock(mainFunction->exitFragment.builder);

  builder.insertFunction(mainFunction->builder, mainFunction->getResultType(),
                         spv::FunctionControlMask::MaskNone,
                         mainFunction->getFunctionType());

  if (stage == Stage::Vertex) {
    builder.createEntryPoint(spv::ExecutionModel::Vertex,
                             mainFunction->builder.id, "main",
                             ctxt.getInterfaces());
  } else if (stage == Stage::Fragment) {
    builder.createEntryPoint(spv::ExecutionModel::Fragment,
                             mainFunction->builder.id, "main",
                             ctxt.getInterfaces());
    builder.createExecutionMode(mainFunction->builder.id,
                                spv::ExecutionMode::OriginUpperLeft, {});
  } else if (stage == Stage::Compute) {
    builder.createEntryPoint(spv::ExecutionModel::GLCompute,
                             mainFunction->builder.id, "main",
                             ctxt.getInterfaces());
    builder.createExecutionMode(mainFunction->builder.id,
                                spv::ExecutionMode::LocalSize,
                                {{dimX, dimY, dimZ}});
  }

  result.spirv = ctxt.getBuilder().build(SPV_VERSION, 0);
  return result;
}
