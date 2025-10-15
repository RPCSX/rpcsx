#include <gtest/gtest.h>
#include <memory>

// Include shader framework for CFG testing
#include "shader/SpvConverter.hpp"
#include "shader/analyze.hpp"
#include "shader/dialect.hpp"
#include "shader/ir.hpp"
#include "shader/ir/Context.hpp"
#include "shader/spv.hpp"
#include "shader/transform.hpp"

using namespace shader;
using Builder = ir::Builder<ir::spv::Builder>;

class GcnShaderTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Setup SPIR-V context for CFG testing
    context = std::make_unique<spv::Context>();
    loc = context->getUnknownLocation();
    trueV = context->getTrue();
    falseV = context->getFalse();
  }

  void TearDown() override { context.reset(); }

  ir::Value createLabel(const std::string &name) {
    auto builder = Builder::createAppend(
        *context, context->layout.getOrCreateFunctions(*context));
    auto label = builder.createSpvLabel(loc);
    context->ns.setNameOf(label, name);
    return label;
  }

  void createBranch(ir::Value from, ir::Value to) {
    Builder::createInsertAfter(*context, from).createSpvBranch(loc, to);
  }

  void createConditionalBranch(ir::Value from, ir::Value a, ir::Value b) {
    Builder::createInsertAfter(*context, from)
        .createSpvBranchConditional(loc, trueV, a, b);
  }

  void createReturn(ir::Value from) {
    Builder::createInsertAfter(*context, from).createSpvReturn(loc);
  }

  void createSwitch(ir::Value from, std::span<const ir::Value> cases) {
    auto globals = Builder::createAppend(
        *context, context->layout.getOrCreateGlobals(*context));
    auto globalVariable = globals.createSpvVariable(
        loc, context->getTypeUInt32(), ir::spv::StorageClass::Private,
        context->imm32(0));

    auto switchOp = Builder::createInsertAfter(*context, from)
                        .createSpvSwitch(loc, globalVariable, cases[0]);

    std::uint32_t i = 0;
    for (auto c : cases.subspan(1)) {
      switchOp.addOperand(i++);
      switchOp.addOperand(c);
    }
  }

  void createSwitchBranch(ir::Value from, ir::Value defaultTarget, 
                         const std::vector<std::pair<std::uint32_t, ir::Value>>& cases) {
    // Create a switch value (use a constant for testing)
    auto type = context->getTypeUInt32();
    auto globals = Builder::createAppend(
        *context, context->layout.getOrCreateGlobals(*context));
    auto globalVariable = globals.createSpvConstant(
        loc, type, 0);

    auto builder = Builder::createInsertAfter(*context, from);
    auto switchInst =
        builder.createSpvSwitch(loc, globalVariable, defaultTarget);

    // Add each case
    for (const auto& [value, target] : cases) {
      switchInst.addOperand(value);
      switchInst.addOperand(target);
    }
  }

  bool testStructurization() {
    auto region = context->layout.getOrCreateFunctions(*context);
    context->layout.regions[spv::BinaryLayout::kFunctions] = {};
    auto functions = context->layout.getOrCreateFunctions(*context);

    structurizeCfg(*context, region);

    {
      auto debugs = Builder::createAppend(
          *context, context->layout.getOrCreateDebugs(*context));

      auto cfg = buildCFG(region.getFirst());
      for (auto node : cfg.getPreorderNodes()) {
        auto value = node->getLabel();
        if (auto name = context->ns.tryGetNameOf(value); !name.empty()) {
          debugs.createSpvName(loc, value, std::string(name));
        }
      }

      for (auto bb : cfg.getPreorderNodes()) {
        for (auto child : bb->range()) {
          child.erase();
          functions.addChild(child);
        }
      }
    }
    region = functions;

    auto entryLabel = region.getFirst().cast<ir::Value>();

    auto memModel = Builder::createAppend(
        *context, context->layout.getOrCreateMemoryModels(*context));
    auto capabilities = Builder::createAppend(
        *context, context->layout.getOrCreateCapabilities(*context));

    capabilities.createSpvCapability(loc, ir::spv::Capability::Shader);

    memModel.createSpvMemoryModel(loc, ir::spv::AddressingModel::Logical,
                                  ir::spv::MemoryModel::GLSL450);

    auto mainReturnT = context->getTypeVoid();
    auto mainFnT = context->getTypeFunction(mainReturnT, {});

    auto builder = Builder::createPrepend(*context, region);
    auto mainFn = builder.createSpvFunction(
        loc, mainReturnT, ir::spv::FunctionControl::None, mainFnT);

    builder.createSpvLabel(loc);
    builder.createSpvBranch(loc, entryLabel);

    Builder::createAppend(*context, region).createSpvFunctionEnd(loc);

    auto entryPoints = Builder::createAppend(
        *context, context->layout.getOrCreateEntryPoints(*context));

    auto executionModes = Builder::createAppend(
        *context, context->layout.getOrCreateExecutionModes(*context));

    executionModes.createSpvExecutionMode(
        mainFn.getLocation(), mainFn,
        ir::spv::ExecutionMode::LocalSize(1, 1, 1));

    entryPoints.createSpvEntryPoint(mainFn.getLocation(),
                                    ir::spv::ExecutionModel::GLCompute, mainFn,
                                    "main", {});

    auto spv = shader::spv::serialize(context->layout.merge(*context));
    if (shader::spv::validate(spv)) {
      return true;
    }

    shader::spv::dump(spv, true);
    return false;
  }

protected:
  std::unique_ptr<spv::Context> context;
  ir::Location loc;
  ir::Value trueV;
  ir::Value falseV;
};

TEST_F(GcnShaderTest, ProjectDivaTest1) {
  auto _1 =  createLabel("1");
  auto _2 =  createLabel("2");
  auto _3 =  createLabel("3");
  auto _4 =  createLabel("4");
  auto _5 =  createLabel("5");
  auto _6 =  createLabel("6");
  auto _7 =  createLabel("7");
  auto _8 =  createLabel("8");
  auto _9 =  createLabel("9");
  auto _10 =  createLabel("10");
  auto _11 =  createLabel("11");
  auto _12 =  createLabel("12");
  auto _13 =  createLabel("13");
  createBranch(_1, _2);
  createConditionalBranch(_2, _4, _3);
  createConditionalBranch(_3, _12, _11);
  createConditionalBranch(_4, _6, _5);
  createConditionalBranch(_5, _9, _8);
  createBranch(_6, _7);
  createBranch(_7, _6);
  createBranch(_8, _3);
  createBranch(_9, _10);
  createBranch(_10, _7);
  createBranch(_11, _12);
  createBranch(_12, _13);
  createReturn(_13);

  EXPECT_TRUE(testStructurization());
}

// TEST_F(GcnShaderTest, BatmanReturnToArkham1) {
//   auto _1 =  createLabel("1");
//   auto _2 =  createLabel("2");
//   auto _3 =  createLabel("3");
//   auto _4 =  createLabel("4");
//   auto _5 =  createLabel("5");
//   auto _6 =  createLabel("6");
//   auto _7 =  createLabel("7");
//   auto _8 =  createLabel("8");
//   auto _9 =  createLabel("9");
//   auto _10 =  createLabel("10");
//   auto _11 =  createLabel("11");
//   auto _12 =  createLabel("12");
//   auto _13 =  createLabel("13");
//   auto _14 =  createLabel("14");
//   auto _15 =  createLabel("15");
//   auto _16 =  createLabel("16");
//   auto _17 =  createLabel("17");
//   auto _18 =  createLabel("18");
//   auto _19 =  createLabel("19");
//   auto _20 =  createLabel("20");
//   auto _21 =  createLabel("21");
//   auto _22 =  createLabel("22");
//   auto _23 =  createLabel("23");
//   auto _24 =  createLabel("24");
//   auto _25 =  createLabel("25");
//   createBranch(_1, _2);
//   createConditionalBranch(_2, _4, _3);
//   createConditionalBranch(_3, _6, _5);
//   createBranch(_4, _3);
//   createConditionalBranch(_5, _8, _7);
//   createBranch(_6, _5);
//   createConditionalBranch(_7, _10, _9);
//   createBranch(_8, _7);
//   createConditionalBranch(_9, _12, _11);
//   createBranch(_10, _9);
//   createConditionalBranch(_11, _14, _13);
//   createBranch(_12, _11);
//   createConditionalBranch(_13, _16, _15);
//   createBranch(_14, _13);
//   createBranch(_15, _25);
//   createConditionalBranch(_16, _18, _17);
//   createBranch(_17, _18);
//   createConditionalBranch(_18, _20, _19);
//   createBranch(_19, _20);
//   createConditionalBranch(_20, _22, _21);
//   createBranch(_21, _22);
//   createConditionalBranch(_22, _24, _23);
//   createBranch(_23, _24);
//   createBranch(_24, _15);
//   createReturn(_25);

//   EXPECT_TRUE(testStructurization());
// }
