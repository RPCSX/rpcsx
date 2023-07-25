#include "CfBuilder.hpp"
#include "Instruction.hpp"
#include <amdgpu/RemoteMemory.hpp>
#include <cassert>
#include <unordered_set>

using namespace amdgpu;
using namespace amdgpu::shader;

struct CfgBuilder {
  cf::Context *context;
  RemoteMemory memory;

  std::size_t analyzeBb(cf::BasicBlock *bb, std::uint64_t *successors,
                        std::size_t *successorsCount) {
    auto address = bb->getAddress();
    auto instBegin = memory.getPointer<std::uint32_t>(address);
    auto instHex = instBegin;

    while (true) {
      auto instruction = Instruction(instHex);
      auto size = instruction.size();
      auto pc = address + ((instHex - instBegin) << 2);
      instHex += size;

      if (instruction.instClass == InstructionClass::Sop1) {
        Sop1 sop1{instHex - size};

        if (sop1.op == Sop1::Op::S_SETPC_B64 ||
            sop1.op == Sop1::Op::S_SWAPPC_B64) {
          bb->createBranchToUnknown();
          break;
        }

        continue;
      }

      if (instruction.instClass == InstructionClass::Sopp) {
        Sopp sopp{instHex - size};

        if (sopp.op == Sopp::Op::S_ENDPGM) {
          bb->createReturn();
          break;
        }

        bool isEnd = false;
        switch (sopp.op) {
        case Sopp::Op::S_BRANCH:
          successors[0] = pc + ((size + sopp.simm) << 2);
          *successorsCount = 1;

          isEnd = true;
          break;

        case Sopp::Op::S_CBRANCH_SCC0:
        case Sopp::Op::S_CBRANCH_SCC1:
        case Sopp::Op::S_CBRANCH_VCCZ:
        case Sopp::Op::S_CBRANCH_VCCNZ:
        case Sopp::Op::S_CBRANCH_EXECZ:
        case Sopp::Op::S_CBRANCH_EXECNZ:
          successors[0] = pc + ((size + sopp.simm) << 2);
          successors[1] = pc + (size << 2);
          *successorsCount = 2;
          isEnd = true;
          break;

        default:
          break;
        }

        if (isEnd) {
          break;
        }
        continue;
      }

      // move instruction that requires EXEC test to separate bb
      if (instruction.instClass == InstructionClass::Vop2 ||
          instruction.instClass == InstructionClass::Vop3 ||
          instruction.instClass == InstructionClass::Mubuf ||
          instruction.instClass == InstructionClass::Mtbuf ||
          instruction.instClass == InstructionClass::Mimg ||
          instruction.instClass == InstructionClass::Ds ||
          instruction.instClass == InstructionClass::Vintrp ||
          instruction.instClass == InstructionClass::Exp ||
          instruction.instClass == InstructionClass::Vop1 ||
          instruction.instClass == InstructionClass::Vopc ||
          instruction.instClass == InstructionClass::Smrd) {
        *successorsCount = 1;

        if (instBegin != instHex - size) {
          // if it is not first instruction in block, move end to prev
          // instruction, successor is current instruction
          instHex -= size;
          successors[0] = pc;
          break;
        }

        successors[0] = pc + (size << 2);
        break;
      }
    }

    return (instHex - instBegin) << 2;
  }

  cf::BasicBlock *buildCfg(std::uint64_t entryPoint) {
    std::vector<std::uint64_t> workList;
    workList.push_back(entryPoint);
    std::unordered_set<std::uint64_t> processed;
    processed.insert(entryPoint);

    struct BranchInfo {
      std::uint64_t source;
      std::size_t count;
      std::uint64_t targets[2];
    };

    std::vector<BranchInfo> branches;

    while (!workList.empty()) {
      auto address = workList.back();
      workList.pop_back();

      auto bb = context->getOrCreateBasicBlock(address);

      if (bb->getSize() != 0) {
        continue;
      }

      std::uint64_t successors[2];
      std::size_t successorsCount = 0;
      std::size_t size = analyzeBb(bb, successors, &successorsCount);
      bb->setSize(size);

      if (successorsCount == 2) {
        branches.push_back(
            {address + size - 4, 2, {successors[0], successors[1]}});

        if (processed.insert(successors[0]).second) {
          workList.push_back(successors[0]);
        }
        if (processed.insert(successors[1]).second) {
          workList.push_back(successors[1]);
        }
      } else if (successorsCount == 1) {
        branches.push_back({address + size - 4, 1, {successors[0]}});

        if (processed.insert(successors[0]).second) {
          workList.push_back(successors[0]);
        }
      }
    }

    for (auto branch : branches) {
      auto bb = context->getBasicBlock(branch.source);
      assert(bb);
      if (branch.count == 2) {
        bb->createConditionalBranch(
            context->getBasicBlockAt(branch.targets[0]),
            context->getBasicBlockAt(branch.targets[1]));
      } else {
        bb->createBranch(context->getBasicBlockAt(branch.targets[0]));
      }
    }

    return context->getBasicBlockAt(entryPoint);
  }
};

cf::BasicBlock *amdgpu::shader::buildCf(cf::Context &ctxt, RemoteMemory memory,
                                        std::uint64_t entryPoint) {
  CfgBuilder builder;
  builder.context = &ctxt;
  builder.memory = memory;

  return builder.buildCfg(entryPoint);
}
