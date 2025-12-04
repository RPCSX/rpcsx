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

      for (auto inst : region.children()) {
        if (auto value = inst.cast<ir::Value>()) {
          if (auto name = context->ns.tryGetNameOf(value); !name.empty()) {
            debugs.createSpvName(loc, value, std::string(name));
          }
        }

        inst.erase();
        functions.addChild(inst);
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
  createConditionalBranch(_6, _7, _8);
  createBranch(_7, _6);
  createBranch(_8, _3);
  createBranch(_9, _10);
  createBranch(_10, _7);
  createBranch(_11, _12);
  createBranch(_12, _13);
  createReturn(_13);

  EXPECT_TRUE(testStructurization());
}

TEST_F(GcnShaderTest, BatmanReturnToArkham1) {
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
  auto _14 =  createLabel("14");
  auto _15 =  createLabel("15");
  auto _16 =  createLabel("16");
  auto _17 =  createLabel("17");
  auto _18 =  createLabel("18");
  auto _19 =  createLabel("19");
  auto _20 =  createLabel("20");
  auto _21 =  createLabel("21");
  auto _22 =  createLabel("22");
  auto _23 =  createLabel("23");
  auto _24 =  createLabel("24");
  auto _25 =  createLabel("25");
  createBranch(_1, _2);
  createConditionalBranch(_2, _4, _3);
  createConditionalBranch(_3, _6, _5);
  createBranch(_4, _3);
  createConditionalBranch(_5, _8, _7);
  createBranch(_6, _5);
  createConditionalBranch(_7, _10, _9);
  createBranch(_8, _7);
  createConditionalBranch(_9, _12, _11);
  createBranch(_10, _9);
  createConditionalBranch(_11, _14, _13);
  createBranch(_12, _11);
  createConditionalBranch(_13, _16, _15);
  createBranch(_14, _13);
  createBranch(_15, _25);
  createConditionalBranch(_16, _18, _17);
  createBranch(_17, _18);
  createConditionalBranch(_18, _20, _19);
  createBranch(_19, _20);
  createConditionalBranch(_20, _22, _21);
  createBranch(_21, _22);
  createConditionalBranch(_22, _24, _23);
  createBranch(_23, _24);
  createBranch(_24, _15);
  createReturn(_25);

  EXPECT_TRUE(testStructurization());
}

TEST_F(GcnShaderTest, Shadow1) {
  auto _1 = createLabel("1");
  auto _2 = createLabel("2");
  auto _3 = createLabel("3");
  auto _4 = createLabel("4");
  auto _5 = createLabel("5");
  auto _6 = createLabel("6");
  auto _7 = createLabel("7");
  auto _8 = createLabel("8");
  auto _9 = createLabel("9");
  auto _10 = createLabel("10");
  auto _11 = createLabel("11");
  auto _12 = createLabel("12");
  auto _13 = createLabel("13");
  auto _14 = createLabel("14");
  auto _15 = createLabel("15");
  auto _16 = createLabel("16");
  auto _17 = createLabel("17");
  auto _18 = createLabel("18");
  auto _19 = createLabel("19");
  auto _20 = createLabel("20");
  auto _21 = createLabel("21");
  auto _22 = createLabel("22");
  auto _23 = createLabel("23");
  auto _24 = createLabel("24");
  auto _25 = createLabel("25");
  auto _26 = createLabel("26");
  auto _27 = createLabel("27");
  auto _28 = createLabel("28");
  auto _29 = createLabel("29");
  auto _30 = createLabel("30");
  auto _31 = createLabel("31");
  auto _32 = createLabel("32");
  auto _33 = createLabel("33");
  auto _34 = createLabel("34");
  auto _35 = createLabel("35");
  auto _36 = createLabel("36");
  auto _37 = createLabel("37");
  auto _38 = createLabel("38");
  auto _39 = createLabel("39");
  auto _40 = createLabel("40");
  auto _41 = createLabel("41");
  auto _42 = createLabel("42");
  auto _43 = createLabel("43");
  auto _44 = createLabel("44");
  auto _45 = createLabel("45");
  auto _46 = createLabel("46");
  auto _47 = createLabel("47");
  auto _48 = createLabel("48");
  auto _49 = createLabel("49");
  auto _50 = createLabel("50");
  auto _51 = createLabel("51");
  auto _52 = createLabel("52");
  auto _53 = createLabel("53");
  auto _54 = createLabel("54");
  auto _55 = createLabel("55");
  auto _56 = createLabel("56");
  auto _57 = createLabel("57");
  auto _58 = createLabel("58");
  auto _59 = createLabel("59");
  auto _60 = createLabel("60");
  auto _61 = createLabel("61");
  createBranch(_1, _2);
  createConditionalBranch(_2, _4, _3);
  createConditionalBranch(_3, _61, _60);
  createConditionalBranch(_4, _6, _5);
  createConditionalBranch(_5, _28, _27);
  createConditionalBranch(_6, _8, _7);
  createConditionalBranch(_7, _18, _17);
  createConditionalBranch(_8, _10, _9);
  createBranch(_9, _7);
  createConditionalBranch(_10, _12, _11);
  createBranch(_11, _7);
  createConditionalBranch(_12, _14, _13);
  createBranch(_13, _7);
  createConditionalBranch(_14, _16, _15);
  createBranch(_15, _7);
  createBranch(_16, _15);
  createConditionalBranch(_17, _20, _19);
  createBranch(_18, _5);
  createBranch(_19, _5);
  createConditionalBranch(_20, _22, _21);
  createBranch(_21, _5);
  createConditionalBranch(_22, _24, _23);
  createBranch(_23, _21);
  createConditionalBranch(_24, _26, _25);
  createBranch(_25, _21);
  createBranch(_26, _25);
  createConditionalBranch(_27, _30, _29);
  createBranch(_28, _27);
  createConditionalBranch(_29, _50, _49);
  createConditionalBranch(_30, _32, _31);
  createConditionalBranch(_31, _29, _3);
  createConditionalBranch(_32, _34, _33);
  createConditionalBranch(_33, _48, _3);
  createConditionalBranch(_34, _36, _35);
  createConditionalBranch(_35, _47, _3);
  createConditionalBranch(_36, _38, _37);
  createConditionalBranch(_37, _46, _3);
  createConditionalBranch(_38, _40, _39);
  createConditionalBranch(_39, _45, _3);
  createConditionalBranch(_40, _42, _41);
  createConditionalBranch(_41, _44, _3);
  createConditionalBranch(_42, _43, _3);
  createBranch(_43, _29);
  createBranch(_44, _29);
  createBranch(_45, _29);
  createBranch(_46, _29);
  createBranch(_47, _29);
  createBranch(_48, _29);
  createConditionalBranch(_49, _58, _57);
  createConditionalBranch(_50, _52, _51);
  createConditionalBranch(_51, _49, _3);
  createConditionalBranch(_52, _54, _53);
  createConditionalBranch(_53, _56, _3);
  createConditionalBranch(_54, _55, _3);
  createBranch(_55, _49);
  createBranch(_56, _49);
  createBranch(_57, _58);
  createBranch(_58, _59);
  createReturn(_59);
  createBranch(_60, _61);
  createBranch(_61, _59);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test conditional CFG structurization
 */
TEST_F(GcnShaderTest, ConditionalCfgStructurization) {
  auto _1 = createLabel("1");
  auto _2 = createLabel("2");
  auto _3 = createLabel("3");
  auto _4 = createLabel("4");

  // Build conditional CFG: 1 -> if(2,3), 2 -> 4, 3 -> 4
  createConditionalBranch(_1, _2, _3);
  createBranch(_2, _4);
  createBranch(_3, _4);
  createReturn(_4);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop CFG structurization
 */
TEST_F(GcnShaderTest, LoopCfgStructurization) {
  // Create simple loop: 1 -> 2 -> conditional back to 1 or forward to 3
  auto _1 = createLabel("1");
  auto _2 = createLabel("2");
  auto _3 = createLabel("3");

  createBranch(_1, _2);
  createConditionalBranch(_2, _1, _3);
  createReturn(_3);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test that new implementation doesn't crash with complex patterns
 */
TEST_F(GcnShaderTest, ComplexPatternResilience) {
  // Create a more complex CFG and ensure it doesn't crash
  auto _1 = createLabel("1");
  auto _2 = createLabel("2");
  auto _3 = createLabel("3");

  createBranch(_1, _2);
  createBranch(_2, _3);
  createReturn(_3);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test academic algorithm forward goto transformation
 */
TEST_F(GcnShaderTest, ForwardGotoTransformation) {
  // Test the academic algorithm's handling of forward gotos
  auto _1 = createLabel("1");
  auto _2 = createLabel("2");

  createBranch(_1, _2);
  createReturn(_2);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test nested loops CFG
 */
TEST_F(GcnShaderTest, NestedLoopCfgStructurization) {
  auto entry = createLabel("entry");
  auto outer_loop = createLabel("outer_loop");
  auto inner_loop = createLabel("inner_loop");
  auto inner_body = createLabel("inner_body");
  auto exit = createLabel("exit");

  // Build nested loop CFG
  createBranch(entry, outer_loop);
  createConditionalBranch(outer_loop, inner_loop, exit);
  createConditionalBranch(inner_loop, outer_loop, inner_body);
  createConditionalBranch(inner_body, inner_loop, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

TEST_F(GcnShaderTest, LoopWithHeaderCfgStructurization) {
  // Create nested loops: outer -> inner -> inner_body -> inner (back), inner
  // -> outer (back), outer -> exit

  auto outer_loop = createLabel("outer_loop");
  auto inner_loop = createLabel("inner_loop");
  auto inner_body = createLabel("inner_body");
  auto exit = createLabel("exit");

  // Build nested loop CFG
  createBranch(outer_loop, inner_loop);
  createConditionalBranch(inner_loop, outer_loop, inner_body);
  createConditionalBranch(inner_body, inner_loop, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test irreducible CFG (multiple entry loop)
 */
TEST_F(GcnShaderTest, IrreducibleCfgStructurization) {
  // Create irreducible: entry -> if(A, B), A -> C, B -> C, C -> if(A, exit)

  auto entry = createLabel("entry");
  auto A = createLabel("A");
  auto B = createLabel("B");
  auto C = createLabel("C");
  auto exit = createLabel("exit");

  createConditionalBranch(entry, A, B);
  createBranch(A, C);
  createBranch(B, C);
  createConditionalBranch(C, A, exit); // Back edge to A
  createReturn(exit);
  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch-like CFG
 */
TEST_F(GcnShaderTest, SwitchLikeCfgStructurization) {
  // Create switch-like: entry -> if(case1, case2), case1 -> merge, case2 ->
  // if(case3, merge), case3 -> merge
  auto entry = createLabel("entry");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto merge = createLabel("merge");

  createConditionalBranch(entry, case1, case2);
  createBranch(case1, merge);
  createConditionalBranch(case2, case3, merge);
  createBranch(case3, merge);
  createReturn(merge);
  EXPECT_TRUE(testStructurization());
}

/**
 * Test academic algorithm data structures (verify no regression)
 */
TEST_F(GcnShaderTest, AcademicAlgorithmDataStructures) {
  // Create a basic CFG and ensure structure is created
  auto _1 = createLabel("1");
  auto _2 = createLabel("2");
  auto _3 = createLabel("3");
  auto _4 = createLabel("4");

  createBranch(_1, _2);
  createReturn(_2);

  createBranch(_3, _4);
  createReturn(_4);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test deeply nested loops (3 levels)
 */
TEST_F(GcnShaderTest, DeeplyNestedLoops) {
  auto outer = createLabel("outer");
  auto middle = createLabel("middle");
  auto inner = createLabel("inner");
  auto body = createLabel("body");
  auto exit = createLabel("exit");

  createBranch(outer, middle);
  createConditionalBranch(middle, inner, outer); // middle can exit to outer
  createConditionalBranch(inner, body, middle);  // inner can exit to middle
  createConditionalBranch(body, inner, exit);    // body loops back or exits
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with multiple exits
 */
TEST_F(GcnShaderTest, LoopWithMultipleExits) {
  auto header = createLabel("header");
  auto body1 = createLabel("body1");
  auto body2 = createLabel("body2");
  auto exit1 = createLabel("exit1");
  auto exit2 = createLabel("exit2");
  auto final_exit = createLabel("final_exit");

  createConditionalBranch(header, body1, body2);
  createConditionalBranch(body1, header, exit1); // loop or exit
  createConditionalBranch(body2, header, exit2); // loop or exit
  createBranch(exit1, final_exit);
  createBranch(exit2, final_exit);
  createReturn(final_exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with early return from inner block
 */
TEST_F(GcnShaderTest, LoopWithEarlyReturn) {
  auto header = createLabel("header");
  auto check = createLabel("check");
  auto body = createLabel("body");
  auto exit = createLabel("exit");

  createBranch(header, check);
  createConditionalBranch(check, body, exit);
  createConditionalBranch(body, header, exit); // loop back or return
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test irreducible CFG with multiple entry points
 */
TEST_F(GcnShaderTest, IrreducibleMultipleEntry) {
  auto entry = createLabel("entry");
  auto A = createLabel("A");
  auto B = createLabel("B");
  auto C = createLabel("C");
  auto D = createLabel("D");
  auto exit = createLabel("exit");

  createConditionalBranch(entry, A, B);
  createBranch(A, C);
  createBranch(B, D);
  createConditionalBranch(C, D, A);    // C can go to D or back to A
  createConditionalBranch(D, C, exit); // D can go to C or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch-like structure with fall-through
 */
TEST_F(GcnShaderTest, SwitchWithFallthrough) {
  auto entry = createLabel("entry");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto default_case = createLabel("default");
  auto merge = createLabel("merge");

  createConditionalBranch(entry, case1, case2);
  createConditionalBranch(case1, case3, merge); // case1 can fall through
  createBranch(case2, case3);                   // case2 falls through
  createConditionalBranch(case3, default_case, merge);
  createBranch(default_case, merge);
  createReturn(merge);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with break and continue
 */
TEST_F(GcnShaderTest, LoopWithBreakContinue) {
  auto header = createLabel("header");
  auto condition = createLabel("condition");
  auto body = createLabel("body");
  auto continue_block = createLabel("continue_block");
  auto exit = createLabel("exit");

  createBranch(header, condition);
  createConditionalBranch(condition, body, exit);      // condition check
  createConditionalBranch(body, continue_block, exit); // break or continue
  createBranch(continue_block, header); // continue - back to header
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test diamond pattern inside loop
 */
TEST_F(GcnShaderTest, DiamondInLoop) {
  auto header = createLabel("header");
  auto diamond_entry = createLabel("diamond_entry");
  auto left = createLabel("left");
  auto right = createLabel("right");
  auto diamond_merge = createLabel("diamond_merge");
  auto exit = createLabel("exit");

  createBranch(header, diamond_entry);
  createConditionalBranch(diamond_entry, left, right);
  createBranch(left, diamond_merge);
  createBranch(right, diamond_merge);
  createConditionalBranch(diamond_merge, header, exit); // loop or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop inside diamond
 */
TEST_F(GcnShaderTest, LoopInDiamond) {
  auto entry = createLabel("entry");
  auto left_branch = createLabel("left_branch");
  auto right_branch = createLabel("right_branch");
  auto loop_header = createLabel("loop_header");
  auto loop_body = createLabel("loop_body");
  auto merge = createLabel("merge");

  createConditionalBranch(entry, left_branch, right_branch);
  createBranch(left_branch, loop_header);
  createBranch(right_branch, merge);
  createConditionalBranch(loop_header, loop_body, merge);
  createBranch(loop_body, loop_header); // loop back
  createReturn(merge);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test crossing edges pattern
 */
TEST_F(GcnShaderTest, CrossingEdges) {
  auto entry = createLabel("entry");
  auto A = createLabel("A");
  auto B = createLabel("B");
  auto C = createLabel("C");
  auto D = createLabel("D");
  auto exit = createLabel("exit");

  createConditionalBranch(entry, A, B);
  createConditionalBranch(A, C, D); // A goes to C or D
  createConditionalBranch(B, D, C); // B goes to D or C (crossing)
  createBranch(C, exit);
  createBranch(D, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test multiple nested loops with shared exit
 */
TEST_F(GcnShaderTest, MultipleLoopsSharedExit) {
  auto entry = createLabel("entry");
  auto loop1_header = createLabel("loop1_header");
  auto loop1_body = createLabel("loop1_body");
  auto loop2_header = createLabel("loop2_header");
  auto loop2_body = createLabel("loop2_body");
  auto shared_exit = createLabel("shared_exit");

  createConditionalBranch(entry, loop1_header, loop2_header);
  createConditionalBranch(loop1_header, loop1_body, shared_exit);
  createBranch(loop1_body, loop1_header);
  createConditionalBranch(loop2_header, loop2_body, shared_exit);
  createBranch(loop2_body, loop2_header);
  createReturn(shared_exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test while-do-while combined pattern
 */
TEST_F(GcnShaderTest, WhileDoWhileCombined) {
  auto entry = createLabel("entry");
  auto while_check = createLabel("while_check");
  auto while_body = createLabel("while_body");
  auto do_body = createLabel("do_body");
  auto do_check = createLabel("do_check");
  auto exit = createLabel("exit");

  createBranch(entry, while_check);
  createConditionalBranch(while_check, while_body, exit);
  createBranch(while_body, do_body);
  createBranch(do_body, do_check);
  createConditionalBranch(do_check, do_body,
                          while_check); // do-while + back to while
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with exception-like exit
 */
TEST_F(GcnShaderTest, LoopWithExceptionExit) {
  auto header = createLabel("header");
  auto try_block = createLabel("try_block");
  auto catch_block = createLabel("catch_block");
  auto normal_exit = createLabel("normal_exit");
  auto exception_exit = createLabel("exception_exit");
  auto final_exit = createLabel("final_exit");

  createBranch(header, try_block);
  createConditionalBranch(try_block, header, catch_block); // loop or exception
  createConditionalBranch(catch_block, normal_exit, exception_exit);
  createBranch(normal_exit, final_exit);
  createBranch(exception_exit, final_exit);
  createReturn(final_exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test figure-8 pattern (two interconnected loops)
 */
TEST_F(GcnShaderTest, Figure8Pattern) {
  auto entry = createLabel("entry");
  auto loop1_header = createLabel("loop1_header");
  auto loop1_body = createLabel("loop1_body");
  auto loop2_header = createLabel("loop2_header");
  auto loop2_body = createLabel("loop2_body");
  auto exit = createLabel("exit");

  createBranch(entry, loop1_header);
  createConditionalBranch(loop1_header, loop1_body, exit);
  createConditionalBranch(loop1_body, loop1_header,
                          loop2_header); // to loop1 or loop2
  createConditionalBranch(loop2_header, loop2_body, exit);
  createConditionalBranch(loop2_body, loop2_header,
                          loop1_header); // to loop2 or loop1
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test complex irreducible with 4-node cycle
 */
TEST_F(GcnShaderTest, ComplexIrreducible4Node) {
  auto entry = createLabel("entry");
  auto A = createLabel("A");
  auto B = createLabel("B");
  auto C = createLabel("C");
  auto D = createLabel("D");
  auto exit = createLabel("exit");

  createBranch(entry, A);
  createConditionalBranch(A, B, C);
  createBranch(B, D);
  createConditionalBranch(C, D, A);    // back to A
  createConditionalBranch(D, B, exit); // back to B or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test nested loops with cross-loop edges
 */
TEST_F(GcnShaderTest, NestedLoopsWithCrossEdges) {
  auto outer_header = createLabel("outer_header");
  auto outer_body = createLabel("outer_body");
  auto inner_header = createLabel("inner_header");
  auto inner_body = createLabel("inner_body");
  auto cross_block = createLabel("cross_block");
  auto exit = createLabel("exit");

  createBranch(outer_header, outer_body);
  createConditionalBranch(outer_body, inner_header, cross_block);
  createConditionalBranch(inner_header, inner_body,
                          outer_header); // inner to outer
  createConditionalBranch(inner_body, inner_header,
                          cross_block); // inner loop or cross
  createConditionalBranch(cross_block, outer_header,
                          exit); // back to outer or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test parallel diamond patterns
 */
TEST_F(GcnShaderTest, ParallelDiamonds) {
  auto entry = createLabel("entry");
  auto diamond1_entry = createLabel("diamond1_entry");
  auto diamond1_left = createLabel("diamond1_left");
  auto diamond1_right = createLabel("diamond1_right");
  auto diamond1_merge = createLabel("diamond1_merge");
  auto diamond2_entry = createLabel("diamond2_entry");
  auto diamond2_left = createLabel("diamond2_left");
  auto diamond2_right = createLabel("diamond2_right");
  auto diamond2_merge = createLabel("diamond2_merge");
  auto final_merge = createLabel("final_merge");

  createConditionalBranch(entry, diamond1_entry, diamond2_entry);

  // Diamond 1
  createConditionalBranch(diamond1_entry, diamond1_left, diamond1_right);
  createBranch(diamond1_left, diamond1_merge);
  createBranch(diamond1_right, diamond1_merge);
  createBranch(diamond1_merge, final_merge);

  // Diamond 2
  createConditionalBranch(diamond2_entry, diamond2_left, diamond2_right);
  createBranch(diamond2_left, diamond2_merge);
  createBranch(diamond2_right, diamond2_merge);
  createBranch(diamond2_merge, final_merge);

  createReturn(final_merge);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with internal switch
 */
TEST_F(GcnShaderTest, LoopWithInternalSwitch) {
  auto header = createLabel("header");
  auto switch_entry = createLabel("switch_entry");
  auto case_a = createLabel("case_a");
  auto case_b = createLabel("case_b");
  auto case_c = createLabel("case_c");
  auto switch_merge = createLabel("switch_merge");
  auto exit = createLabel("exit");

  createBranch(header, switch_entry);
  createConditionalBranch(switch_entry, case_a, case_b);
  createConditionalBranch(case_a, case_c, switch_merge);
  createBranch(case_b, switch_merge);
  createBranch(case_c, switch_merge);
  createConditionalBranch(switch_merge, header, exit); // loop or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test deeply nested conditionals
 */
TEST_F(GcnShaderTest, DeeplyNestedConditionals) {
  auto entry = createLabel("entry");
  auto cond1 = createLabel("cond1");
  auto cond2 = createLabel("cond2");
  auto cond3 = createLabel("cond3");
  auto leaf1 = createLabel("leaf1");
  auto leaf2 = createLabel("leaf2");
  auto leaf3 = createLabel("leaf3");
  auto leaf4 = createLabel("leaf4");
  auto merge3 = createLabel("merge3");
  auto merge2 = createLabel("merge2");
  auto merge1 = createLabel("merge1");

  createConditionalBranch(entry, cond1, merge1);
  createConditionalBranch(cond1, cond2, merge2);
  createConditionalBranch(cond2, cond3, merge3);
  createConditionalBranch(cond3, leaf1, leaf2);
  createBranch(leaf1, merge3);
  createBranch(leaf2, merge3);
  createBranch(merge3, leaf3);
  createBranch(leaf3, merge2);
  createBranch(merge2, leaf4);
  createBranch(leaf4, merge1);
  createReturn(merge1);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with multiple latches
 */
TEST_F(GcnShaderTest, LoopWithMultipleLatches) {
  auto header = createLabel("header");
  auto branch_point = createLabel("branch_point");
  auto latch1 = createLabel("latch1");
  auto latch2 = createLabel("latch2");
  auto body = createLabel("body");
  auto exit = createLabel("exit");

  createBranch(header, branch_point);
  createConditionalBranch(branch_point, latch1, latch2);
  createConditionalBranch(latch1, header, body); // latch1 can loop back
  createConditionalBranch(latch2, header, body); // latch2 can loop back
  createBranch(body, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test triangular loop pattern
 */
TEST_F(GcnShaderTest, TriangularLoop) {
  auto A = createLabel("A");
  auto B = createLabel("B");
  auto C = createLabel("C");
  auto exit = createLabel("exit");

  createConditionalBranch(A, B, exit);
  createConditionalBranch(B, C, A);    // B to C or back to A
  createConditionalBranch(C, A, exit); // C back to A or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with nested exception handling
 */
TEST_F(GcnShaderTest, LoopWithNestedExceptionHandling) {
  auto header = createLabel("header");
  auto try_outer = createLabel("try_outer");
  auto try_inner = createLabel("try_inner");
  auto catch_inner = createLabel("catch_inner");
  auto catch_outer = createLabel("catch_outer");
  auto finally_block = createLabel("finally_block");
  auto exit = createLabel("exit");

  createBranch(header, try_outer);
  createConditionalBranch(try_outer, try_inner, catch_outer);
  createConditionalBranch(try_inner, finally_block, catch_inner);
  createBranch(catch_inner, finally_block);
  createBranch(catch_outer, finally_block);
  createConditionalBranch(finally_block, header, exit); // loop or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test hourglass pattern (two diamonds connected)
 */
TEST_F(GcnShaderTest, HourglassPattern) {
  auto entry = createLabel("entry");
  auto top_left = createLabel("top_left");
  auto top_right = createLabel("top_right");
  auto middle = createLabel("middle");
  auto bottom_left = createLabel("bottom_left");
  auto bottom_right = createLabel("bottom_right");
  auto exit = createLabel("exit");

  createConditionalBranch(entry, top_left, top_right);
  createBranch(top_left, middle);
  createBranch(top_right, middle);
  createConditionalBranch(middle, bottom_left, bottom_right);
  createBranch(bottom_left, exit);
  createBranch(bottom_right, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with break to outer scope
 */
TEST_F(GcnShaderTest, LoopWithBreakToOuter) {
  auto outer_header = createLabel("outer_header");
  auto inner_header = createLabel("inner_header");
  auto inner_body = createLabel("inner_body");
  auto break_check = createLabel("break_check");
  auto outer_continue = createLabel("outer_continue");
  auto exit = createLabel("exit");

  createBranch(outer_header, inner_header);
  createConditionalBranch(inner_header, inner_body, outer_continue);
  createBranch(inner_body, break_check);
  createConditionalBranch(break_check, inner_header,
                          exit); // continue inner or break outer
  createBranch(outer_continue, outer_header);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test complex switch with nested loops
 */
TEST_F(GcnShaderTest, ComplexSwitchWithNestedLoops) {
  auto entry = createLabel("entry");
  auto switch_header = createLabel("switch_header");
  auto case1_loop = createLabel("case1_loop");
  auto case1_body = createLabel("case1_body");
  auto case2_loop = createLabel("case2_loop");
  auto case2_body = createLabel("case2_body");
  auto switch_exit = createLabel("switch_exit");

  createBranch(entry, switch_header);
  createConditionalBranch(switch_header, case1_loop, case2_loop);

  // Case 1 with loop
  createConditionalBranch(case1_loop, case1_body, switch_exit);
  createBranch(case1_body, case1_loop);

  // Case 2 with loop
  createConditionalBranch(case2_loop, case2_body, switch_exit);
  createBranch(case2_body, case2_loop);

  createReturn(switch_exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test ladder pattern (sequential conditions)
 */
TEST_F(GcnShaderTest, LadderPattern) {
  auto entry = createLabel("entry");
  auto check1 = createLabel("check1");
  auto check2 = createLabel("check2");
  auto check3 = createLabel("check3");
  auto action1 = createLabel("action1");
  auto action2 = createLabel("action2");
  auto action3 = createLabel("action3");
  auto final_action = createLabel("final_action");
  auto exit = createLabel("exit");

  createBranch(entry, check1);
  createConditionalBranch(check1, action1, check2);
  createBranch(action1, final_action);
  createConditionalBranch(check2, action2, check3);
  createBranch(action2, final_action);
  createConditionalBranch(check3, action3, final_action);
  createBranch(action3, final_action);
  createBranch(final_action, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test spiral pattern (increasing complexity)
 */
TEST_F(GcnShaderTest, SpiralPattern) {
  auto center = createLabel("center");
  auto ring1 = createLabel("ring1");
  auto ring2 = createLabel("ring2");
  auto ring3 = createLabel("ring3");
  auto connector1 = createLabel("connector1");
  auto connector2 = createLabel("connector2");
  auto exit = createLabel("exit");

  createConditionalBranch(center, ring1, exit);
  createConditionalBranch(ring1, ring2, connector1);
  createConditionalBranch(ring2, ring3, connector2);
  createConditionalBranch(ring3, center, exit); // spiral back to center
  createBranch(connector1, center);
  createBranch(connector2, ring1);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test binary tree traversal pattern
 */
TEST_F(GcnShaderTest, BinaryTreeTraversal) {
  auto root = createLabel("root");
  auto left_subtree = createLabel("left_subtree");
  auto right_subtree = createLabel("right_subtree");
  auto left_leaf = createLabel("left_leaf");
  auto right_leaf = createLabel("right_leaf");
  auto process_left = createLabel("process_left");
  auto process_right = createLabel("process_right");
  auto merge = createLabel("merge");

  createConditionalBranch(root, left_subtree, right_subtree);
  createConditionalBranch(left_subtree, left_leaf, process_left);
  createConditionalBranch(right_subtree, right_leaf, process_right);
  createBranch(left_leaf, merge);
  createBranch(right_leaf, merge);
  createBranch(process_left, merge);
  createBranch(process_right, merge);
  createReturn(merge);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test state machine pattern
 */
TEST_F(GcnShaderTest, StateMachine) {
  auto init_state = createLabel("init_state");
  auto state_a = createLabel("state_a");
  auto state_b = createLabel("state_b");
  auto state_c = createLabel("state_c");
  auto transition = createLabel("transition");
  auto final_state = createLabel("final_state");

  createBranch(init_state, state_a);
  createConditionalBranch(state_a, state_b, transition);
  createConditionalBranch(state_b, state_c, state_a);     // B to C or back to A
  createConditionalBranch(state_c, state_a, final_state); // C to A or final
  createConditionalBranch(transition, state_b, final_state);
  createReturn(final_state);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test mesh pattern (highly connected)
 */
TEST_F(GcnShaderTest, MeshPattern) {
  auto node1 = createLabel("node1");
  auto node2 = createLabel("node2");
  auto node3 = createLabel("node3");
  auto node4 = createLabel("node4");
  auto hub = createLabel("hub");
  auto exit = createLabel("exit");

  createConditionalBranch(node1, node2, node3);
  createConditionalBranch(node2, node4, hub);
  createConditionalBranch(node3, node4, hub);
  createConditionalBranch(node4, node1, hub); // back to node1
  createConditionalBranch(hub, node1, exit);  // back to node1 or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test pipeline pattern (sequential stages)
 */
TEST_F(GcnShaderTest, PipelinePattern) {
  auto input_stage = createLabel("input_stage");
  auto process_stage1 = createLabel("process_stage1");
  auto process_stage2 = createLabel("process_stage2");
  auto process_stage3 = createLabel("process_stage3");
  auto output_stage = createLabel("output_stage");
  auto error_handler = createLabel("error_handler");
  auto retry = createLabel("retry");
  auto exit = createLabel("exit");

  createBranch(input_stage, process_stage1);
  createConditionalBranch(process_stage1, process_stage2, error_handler);
  createConditionalBranch(process_stage2, process_stage3, error_handler);
  createConditionalBranch(process_stage3, output_stage, error_handler);
  createBranch(output_stage, exit);
  createConditionalBranch(error_handler, retry, exit);
  createBranch(retry, input_stage); // retry from beginning
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test fractal-like recursive pattern
 */
TEST_F(GcnShaderTest, FractalPattern) {
  auto entry = createLabel("entry");
  auto level1 = createLabel("level1");
  auto level2 = createLabel("level2");
  auto level3 = createLabel("level3");
  auto recurse_check = createLabel("recurse_check");
  auto base_case = createLabel("base_case");
  auto combine = createLabel("combine");
  auto exit = createLabel("exit");

  createBranch(entry, level1);
  createConditionalBranch(level1, level2, recurse_check);
  createConditionalBranch(level2, level3, base_case);
  createConditionalBranch(level3, base_case, combine);
  createConditionalBranch(recurse_check, level1, combine); // recurse or combine
  createBranch(base_case, combine);
  createConditionalBranch(combine, level1, exit); // recurse again or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test butterfly pattern (symmetric branches that cross)
 */
TEST_F(GcnShaderTest, ButterflyPattern) {
  auto entry = createLabel("entry");
  auto wing1_top = createLabel("wing1_top");
  auto wing1_bottom = createLabel("wing1_bottom");
  auto wing2_top = createLabel("wing2_top");
  auto wing2_bottom = createLabel("wing2_bottom");
  auto body_center = createLabel("body_center");
  auto exit = createLabel("exit");

  createConditionalBranch(entry, wing1_top, wing2_top);
  createConditionalBranch(wing1_top, wing2_bottom,
                          body_center); // cross pattern
  createConditionalBranch(wing2_top, wing1_bottom,
                          body_center); // cross pattern
  createBranch(wing1_bottom, exit);
  createBranch(wing2_bottom, exit);
  createBranch(body_center, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test nested try-catch with multiple exception types
 */
TEST_F(GcnShaderTest, NestedTryCatchMultipleExceptions) {
  auto entry = createLabel("entry");
  auto try_outer = createLabel("try_outer");
  auto try_inner = createLabel("try_inner");
  auto risky_operation = createLabel("risky_operation");
  auto catch_type1 = createLabel("catch_type1");
  auto catch_type2 = createLabel("catch_type2");
  auto catch_all = createLabel("catch_all");
  auto finally_inner = createLabel("finally_inner");
  auto finally_outer = createLabel("finally_outer");
  auto success = createLabel("success");
  auto exit = createLabel("exit");

  createBranch(entry, try_outer);
  createBranch(try_outer, try_inner);
  createBranch(try_inner, risky_operation);
  createConditionalBranch(risky_operation, success, catch_type1);
  createConditionalBranch(catch_type1, finally_inner, catch_type2);
  createConditionalBranch(catch_type2, finally_inner, catch_all);
  createBranch(catch_all, finally_inner);
  createBranch(success, finally_inner);
  createBranch(finally_inner, finally_outer);
  createBranch(finally_outer, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test producer-consumer pattern
 */
TEST_F(GcnShaderTest, ProducerConsumerPattern) {
  auto entry = createLabel("entry");
  auto producer = createLabel("producer");
  auto buffer_check = createLabel("buffer_check");
  auto consumer = createLabel("consumer");
  auto process_item = createLabel("process_item");
  auto sync_point = createLabel("sync_point");
  auto exit = createLabel("exit");

  createConditionalBranch(entry, producer, consumer);
  createBranch(producer, buffer_check);
  createConditionalBranch(buffer_check, consumer,
                          producer); // buffer full, switch
  createBranch(consumer, process_item);
  createConditionalBranch(process_item, sync_point,
                          consumer);                   // process or continue
  createConditionalBranch(sync_point, producer, exit); // sync or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test multi-level break pattern
 */
TEST_F(GcnShaderTest, MultiLevelBreakPattern) {
  auto outer_loop = createLabel("outer_loop");
  auto middle_loop = createLabel("middle_loop");
  auto inner_loop = createLabel("inner_loop");
  auto inner_body = createLabel("inner_body");
  auto break_check = createLabel("break_check");
  auto middle_continue = createLabel("middle_continue");
  auto outer_continue = createLabel("outer_continue");
  auto exit = createLabel("exit");

  createBranch(outer_loop, middle_loop);
  createBranch(middle_loop, inner_loop);
  createBranch(inner_loop, inner_body);
  createBranch(inner_body, break_check);
  createConditionalBranch(break_check, inner_loop,
                          middle_continue); // continue inner or break
  createConditionalBranch(middle_continue, middle_loop,
                          outer_continue); // continue middle or break
  createConditionalBranch(outer_continue, outer_loop,
                          exit); // continue outer or exit
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test parallel processing pattern
 */
TEST_F(GcnShaderTest, ParallelProcessingPattern) {
  auto entry = createLabel("entry");
  auto fork_point = createLabel("fork_point");
  auto thread1 = createLabel("thread1");
  auto thread2 = createLabel("thread2");
  auto thread3 = createLabel("thread3");
  auto work1 = createLabel("work1");
  auto work2 = createLabel("work2");
  auto work3 = createLabel("work3");
  auto barrier = createLabel("barrier");
  auto join_point = createLabel("join_point");
  auto exit = createLabel("exit");

  createBranch(entry, fork_point);
  createConditionalBranch(fork_point, thread1, thread2);
  createConditionalBranch(thread1, work1, thread3);
  createBranch(thread2, work2);
  createBranch(thread3, work3);
  createBranch(work1, barrier);
  createBranch(work2, barrier);
  createBranch(work3, barrier);
  createBranch(barrier, join_point);
  createBranch(join_point, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test recursive descent parser pattern
 */
TEST_F(GcnShaderTest, RecursiveDescentParser) {
  auto parse_expression = createLabel("parse_expression");
  auto parse_term = createLabel("parse_term");
  auto parse_factor = createLabel("parse_factor");
  auto parse_number = createLabel("parse_number");
  auto parse_parentheses = createLabel("parse_parentheses");
  auto operator_check = createLabel("operator_check");
  auto success = createLabel("success");
  auto error = createLabel("error");
  auto exit = createLabel("exit");

  createBranch(parse_expression, parse_term);
  createConditionalBranch(parse_term, parse_factor, operator_check);
  createConditionalBranch(parse_factor, parse_number, parse_parentheses);
  createBranch(parse_number, success);
  createConditionalBranch(parse_parentheses, parse_expression,
                          error); // recursive call
  createConditionalBranch(operator_check, parse_term,
                          success); // continue parsing
  createBranch(success, exit);
  createBranch(error, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test pathological irreducible CFG (worst case)
 */
TEST_F(GcnShaderTest, PathologicalIrreducibleCFG) {
  auto entry = createLabel("entry");
  auto A = createLabel("A");
  auto B = createLabel("B");
  auto C = createLabel("C");
  auto D = createLabel("D");
  auto E = createLabel("E");
  auto F = createLabel("F");
  auto exit = createLabel("exit");

  // Create a highly irreducible pattern where multiple nodes can reach multiple
  // nodes
  createConditionalBranch(entry, A, B);
  createConditionalBranch(A, C, D);
  createConditionalBranch(B, D, E);
  createConditionalBranch(C, E, exit);
  createConditionalBranch(D, F, A); // back to A
  createConditionalBranch(E, A, B); // back to A or B
  createConditionalBranch(F, B, C); // back to B or C
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop with computed goto simulation
 */
TEST_F(GcnShaderTest, ComputedGotoSimulation) {
  auto entry = createLabel("entry");
  auto dispatch_table = createLabel("dispatch_table");
  auto target1 = createLabel("target1");
  auto target2 = createLabel("target2");
  auto target3 = createLabel("target3");
  auto target4 = createLabel("target4");
  auto merge_point = createLabel("merge_point");
  auto loop_back = createLabel("loop_back");
  auto exit = createLabel("exit");

  createBranch(entry, dispatch_table);
  createConditionalBranch(dispatch_table, target1, target2);
  createConditionalBranch(target1, target3, merge_point);
  createConditionalBranch(target2, target4, merge_point);
  createBranch(target3, merge_point);
  createBranch(target4, merge_point);
  createConditionalBranch(merge_point, loop_back, exit);
  createBranch(loop_back, dispatch_table); // computed goto loop
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test coroutine-like pattern with yield points
 */
TEST_F(GcnShaderTest, CoroutineLikePattern) {
  auto entry = createLabel("entry");
  auto state_machine = createLabel("state_machine");
  auto yield_point1 = createLabel("yield_point1");
  auto work1 = createLabel("work1");
  auto yield_point2 = createLabel("yield_point2");
  auto work2 = createLabel("work2");
  auto yield_point3 = createLabel("yield_point3");
  auto final_work = createLabel("final_work");
  auto exit = createLabel("exit");

  createBranch(entry, state_machine);
  createConditionalBranch(state_machine, yield_point1, yield_point2);
  createBranch(yield_point1, work1);
  createConditionalBranch(work1, state_machine,
                          yield_point2); // yield or continue
  createBranch(yield_point2, work2);
  createConditionalBranch(work2, state_machine,
                          yield_point3); // yield or continue
  createBranch(yield_point3, final_work);
  createConditionalBranch(final_work, state_machine, exit); // yield or complete
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test extreme nesting depth (stress test)
 */
TEST_F(GcnShaderTest, ExtremeNestingDepth) {
  auto level0 = createLabel("level0");
  auto level1 = createLabel("level1");
  auto level2 = createLabel("level2");
  auto level3 = createLabel("level3");
  auto level4 = createLabel("level4");
  auto level5 = createLabel("level5");
  auto level6 = createLabel("level6");
  auto level7 = createLabel("level7");
  auto level8 = createLabel("level8");
  auto level9 = createLabel("level9");
  auto deep_core = createLabel("deep_core");
  auto unwind = createLabel("unwind");
  auto exit = createLabel("exit");

  // Create deeply nested structure
  createConditionalBranch(level0, level1, unwind);
  createConditionalBranch(level1, level2, unwind);
  createConditionalBranch(level2, level3, unwind);
  createConditionalBranch(level3, level4, unwind);
  createConditionalBranch(level4, level5, unwind);
  createConditionalBranch(level5, level6, unwind);
  createConditionalBranch(level6, level7, unwind);
  createConditionalBranch(level7, level8, unwind);
  createConditionalBranch(level8, level9, unwind);
  createConditionalBranch(level9, deep_core, unwind);
  createConditionalBranch(deep_core, level0, exit); // back to top or exit
  createBranch(unwind, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

// ========================= COMPREHENSIVE SWITCH PATTERN TESTS
// =========================

/**
 * Test simple switch with 3 cases
 */
TEST_F(GcnShaderTest, SimpleSwitch3Cases) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit, {{0, case0}, {1, case1}, {2, case2}});
  createBranch(case0, exit);
  createBranch(case1, exit);
  createBranch(case2, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with fallthrough patterns
 */
TEST_F(GcnShaderTest, SwitchWithFallthroughPattern) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit,
                     {{0, case0}, {1, case1}, {2, case2}, {3, case3}});
  createBranch(case0, case1); // fallthrough
  createBranch(case1, case2); // fallthrough
  createBranch(case2, exit);  // break
  createBranch(case3, exit);  // break
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with mixed fallthrough and breaks
 */
TEST_F(GcnShaderTest, SwitchMixedFallthroughBreaks) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto case4 = createLabel("case4");
  auto exit = createLabel("exit");

  createSwitchBranch(
      entry, exit,
      {{0, case0}, {1, case1}, {2, case2}, {3, case3}, {4, case4}});
  createBranch(case0, exit);  // break
  createBranch(case1, case2); // fallthrough
  createBranch(case2, case3); // fallthrough
  createBranch(case3, exit);  // break
  createBranch(case4, exit);  // break
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test large switch with 10 cases
 */
TEST_F(GcnShaderTest, LargeSwitch10Cases) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto case4 = createLabel("case4");
  auto case5 = createLabel("case5");
  auto case6 = createLabel("case6");
  auto case7 = createLabel("case7");
  auto case8 = createLabel("case8");
  auto case9 = createLabel("case9");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit,
                     {{0, case0},
                      {1, case1},
                      {2, case2},
                      {3, case3},
                      {4, case4},
                      {5, case5},
                      {6, case6},
                      {7, case7},
                      {8, case8},
                      {9, case9}});
  createBranch(case0, exit);
  createBranch(case1, exit);
  createBranch(case2, exit);
  createBranch(case3, exit);
  createBranch(case4, exit);
  createBranch(case5, exit);
  createBranch(case6, exit);
  createBranch(case7, exit);
  createBranch(case8, exit);
  createBranch(case9, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test very large switch with 25 cases
 */
TEST_F(GcnShaderTest, VeryLargeSwitch25Cases) {
  auto entry = createLabel("entry");
  std::vector<ir::Value> cases;
  for (int i = 0; i < 25; i++) {
    cases.push_back(createLabel("case" + std::to_string(i)));
  }
  auto exit = createLabel("exit");

  // Create switch with 25 cases
  std::vector<std::pair<std::uint32_t, ir::Value>> switch_cases;
  for (int i = 0; i < 25; i++) {
    switch_cases.push_back({i, cases[i]});
  }
  createSwitchBranch(entry, exit, switch_cases);

  // Each case branches to exit
  for (auto case_label : cases) {
    createBranch(case_label, exit);
  }
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test huge switch with 50 cases
 */
TEST_F(GcnShaderTest, HugeSwitch50Cases) {
  auto entry = createLabel("entry");
  std::vector<ir::Value> cases;
  for (int i = 0; i < 50; i++) {
    cases.push_back(createLabel("case" + std::to_string(i)));
  }
  auto exit = createLabel("exit");

  // Create switch with 50 cases
  std::vector<std::pair<std::uint32_t, ir::Value>> switch_cases;
  for (int i = 0; i < 50; i++) {
    switch_cases.push_back({i, cases[i]});
  }
  createSwitchBranch(entry, exit, switch_cases);

  // Each case branches to exit
  for (auto case_label : cases) {
    createBranch(case_label, exit);
  }
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test massive switch with 100 cases
 */
TEST_F(GcnShaderTest, MassiveSwitch100Cases) {
  auto entry = createLabel("entry");
  std::vector<ir::Value> cases;
  for (int i = 0; i < 100; i++) {
    cases.push_back(createLabel("case" + std::to_string(i)));
  }
  auto exit = createLabel("exit");

  // Create switch with 100 cases
  std::vector<std::pair<std::uint32_t, ir::Value>> switch_cases;
  for (int i = 0; i < 100; i++) {
    switch_cases.push_back({i, cases[i]});
  }
  createSwitchBranch(entry, exit, switch_cases);

  // Each case branches to exit
  for (auto case_label : cases) {
    createBranch(case_label, exit);
  }
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with nested conditionals in cases
 */
TEST_F(GcnShaderTest, SwitchWithNestedConditionals) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto nested_true_0 = createLabel("nested_true_0");
  auto nested_false_0 = createLabel("nested_false_0");
  auto nested_true_1 = createLabel("nested_true_1");
  auto nested_false_1 = createLabel("nested_false_1");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit, {{0, case0}, {1, case1}, {2, case2}});

  // Case 0 has nested conditional
  createConditionalBranch(case0, nested_true_0, nested_false_0);
  createBranch(nested_true_0, exit);
  createBranch(nested_false_0, exit);

  // Case 1 has nested conditional
  createConditionalBranch(case1, nested_true_1, nested_false_1);
  createBranch(nested_true_1, exit);
  createBranch(nested_false_1, exit);

  // Case 2 is simple
  createBranch(case2, exit);

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with nested loops in cases
 */
TEST_F(GcnShaderTest, SwitchWithNestedLoops) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto loop_body_0 = createLabel("loop_body_0");
  auto loop_body_1 = createLabel("loop_body_1");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit, {{0, case0}, {1, case1}, {2, case2}});

  // Case 0 has a loop
  createConditionalBranch(case0, loop_body_0, exit);
  createConditionalBranch(loop_body_0, case0, exit); // loop back

  // Case 1 has a loop
  createConditionalBranch(case1, loop_body_1, exit);
  createConditionalBranch(loop_body_1, case1, exit); // loop back

  // Case 2 is simple
  createBranch(case2, exit);

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with sparse case values
 */
TEST_F(GcnShaderTest, SwitchWithSparseCases) {
  auto entry = createLabel("entry");
  auto case1 = createLabel("case1");
  auto case10 = createLabel("case10");
  auto case50 = createLabel("case50");
  auto case100 = createLabel("case100");
  auto case999 = createLabel("case999");
  auto exit = createLabel("exit");

  createSwitchBranch(
      entry, exit,
      {{1, case1}, {10, case10}, {50, case50}, {100, case100}, {999, case999}});
  createBranch(case1, exit);
  createBranch(case10, exit);
  createBranch(case50, exit);
  createBranch(case100, exit);
  createBranch(case999, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with negative case values
 */
TEST_F(GcnShaderTest, SwitchWithNegativeCases) {
  auto entry = createLabel("entry");
  auto case_neg10 = createLabel("case_neg10");
  auto case_neg5 = createLabel("case_neg5");
  auto case0 = createLabel("case0");
  auto case5 = createLabel("case5");
  auto case10 = createLabel("case10");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit,
                     {{-10, case_neg10},
                      {-5, case_neg5},
                      {0, case0},
                      {5, case5},
                      {10, case10}});
  createBranch(case_neg10, exit);
  createBranch(case_neg5, exit);
  createBranch(case0, exit);
  createBranch(case5, exit);
  createBranch(case10, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch inside loop
 */
TEST_F(GcnShaderTest, SwitchInsideLoop) {
  auto entry = createLabel("entry");
  auto loop_header = createLabel("loop_header");
  auto switch_block = createLabel("switch_block");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto loop_continue = createLabel("loop_continue");
  auto exit = createLabel("exit");

  createBranch(entry, loop_header);
  createConditionalBranch(loop_header, switch_block, exit);

  createSwitchBranch(switch_block, loop_continue,
                     {{0, case0}, {1, case1}, {2, case2}});
  createBranch(case0, loop_continue);
  createBranch(case1, exit); // break from loop
  createBranch(case2, loop_continue);

  createBranch(loop_continue, loop_header);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test loop inside switch case
 */
TEST_F(GcnShaderTest, LoopInsideSwitchCase) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto loop_header = createLabel("loop_header");
  auto loop_body = createLabel("loop_body");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit, {{0, case0}, {1, case1}, {2, case2}});

  // Case 0 is simple
  createBranch(case0, exit);

  // Case 1 contains a loop
  createBranch(case1, loop_header);
  createConditionalBranch(loop_header, loop_body, exit);
  createConditionalBranch(loop_body, loop_header, exit);

  // Case 2 is simple
  createBranch(case2, exit);

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test nested switches
 */
TEST_F(GcnShaderTest, NestedSwitches) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto inner_case0 = createLabel("inner_case0");
  auto inner_case1 = createLabel("inner_case1");
  auto inner_case2 = createLabel("inner_case2");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit, {{0, case0}, {1, case1}, {2, case2}});

  // Case 0 is simple
  createBranch(case0, exit);

  // Case 1 contains nested switch
  createSwitchBranch(case1, exit,
                     {{0, inner_case0}, {1, inner_case1}, {2, inner_case2}});
  createBranch(inner_case0, exit);
  createBranch(inner_case1, exit);
  createBranch(inner_case2, exit);

  // Case 2 is simple
  createBranch(case2, exit);

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with complex fallthrough chain
 */
TEST_F(GcnShaderTest, SwitchComplexFallthroughChain) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto case4 = createLabel("case4");
  auto case5 = createLabel("case5");
  auto case6 = createLabel("case6");
  auto case7 = createLabel("case7");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit,
                     {{0, case0},
                      {1, case1},
                      {2, case2},
                      {3, case3},
                      {4, case4},
                      {5, case5},
                      {6, case6},
                      {7, case7}});

  // Complex fallthrough pattern: 0->1->2, 3->break, 4->5->6->7, 7->break
  createBranch(case0, case1); // fallthrough
  createBranch(case1, case2); // fallthrough
  createBranch(case2, exit);  // break
  createBranch(case3, exit);  // break
  createBranch(case4, case5); // fallthrough
  createBranch(case5, case6); // fallthrough
  createBranch(case6, case7); // fallthrough
  createBranch(case7, exit);  // break

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with cross-case jumps (goto simulation)
 */
TEST_F(GcnShaderTest, SwitchWithCrossCaseJumps) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto shared_code = createLabel("shared_code");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit,
                     {{0, case0}, {1, case1}, {3, case3}, {2, case2}});

  // Cases can jump to shared code or each other
  createBranch(case0, shared_code);
  createBranch(case1, case3); // jump to case 3
  createBranch(case2, shared_code);
  createBranch(case3, exit);
  createBranch(shared_code, exit);

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with multiple shared exit points
 */
TEST_F(GcnShaderTest, SwitchMultipleSharedExits) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto exit1 = createLabel("exit1");
  auto exit2 = createLabel("exit2");
  auto final_exit = createLabel("final_exit");

  createSwitchBranch(entry, final_exit,
                     {{0, case0}, {1, case1}, {2, case2}, {3, case3}});

  // Cases branch to different exit points
  createBranch(case0, exit1);
  createBranch(case1, exit1);
  createBranch(case2, exit2);
  createBranch(case3, exit2);

  createBranch(exit1, final_exit);
  createBranch(exit2, final_exit);
  createReturn(final_exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with return statements in cases
 */
TEST_F(GcnShaderTest, SwitchWithReturns) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto case3 = createLabel("case3");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit,
                     {{0, case0}, {1, case1}, {2, case2}, {3, case3}});

  // Some cases return directly, others continue
  createReturn(case0); // direct return
  createBranch(case1, exit);
  createReturn(case2); // direct return
  createBranch(case3, exit);

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with exception-like jumps
 */
TEST_F(GcnShaderTest, SwitchWithExceptionJumps) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto normal_flow = createLabel("normal_flow");
  auto exception_handler = createLabel("exception_handler");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit, {{0, case0}, {1, case1}, {2, case2}});

  // Cases can either continue normally or jump to exception handler
  createConditionalBranch(case0, normal_flow, exception_handler);
  createConditionalBranch(case1, normal_flow, exception_handler);
  createBranch(case2, normal_flow);

  createBranch(normal_flow, exit);
  createBranch(exception_handler, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch-based state machine
 */
TEST_F(GcnShaderTest, SwitchBasedStateMachine) {
  auto entry = createLabel("entry");
  auto header = createLabel("header");
  auto state_machine = createLabel("state_machine");
  auto state0 = createLabel("state0");
  auto state1 = createLabel("state1");
  auto state2 = createLabel("state2");
  auto state3 = createLabel("state3");
  auto update_state = createLabel("update_state");
  auto exit = createLabel("exit");

  createBranch(entry, header);

  // State machine loop
  createConditionalBranch(header, exit, state_machine); // continue condition
  createSwitchBranch(state_machine, exit,
                     {{0, state0}, {1, state1}, {2, state2}, {3, state3}});

  // Each state processes and transitions
  createBranch(state0, update_state);
  createBranch(state1, update_state);
  createBranch(state2, update_state);
  createBranch(state3, exit); // final state

  createBranch(update_state, state_machine); // loop back
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with computed jump table simulation
 */
TEST_F(GcnShaderTest, SwitchComputedJumpTable) {
  auto entry = createLabel("entry");
  auto compute_index = createLabel("compute_index");
  auto jump_table = createLabel("jump_table");
  auto target0 = createLabel("target0");
  auto target1 = createLabel("target1");
  auto target2 = createLabel("target2");
  auto target3 = createLabel("target3");
  auto target4 = createLabel("target4");
  auto merge = createLabel("merge");
  auto exit = createLabel("exit");

  createBranch(entry, compute_index);
  createBranch(compute_index, jump_table);

  // Jump table with 5 targets
  createSwitchBranch(
      jump_table, merge,
      {{0, target0}, {1, target1}, {2, target2}, {3, target3}, {4, target4}});

  // Each target does some work then merges
  createBranch(target0, merge);
  createBranch(target1, merge);
  createBranch(target2, merge);
  createBranch(target3, merge);
  createBranch(target4, merge);

  createBranch(merge, exit);
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test switch with interleaved loops and conditionals
 */
TEST_F(GcnShaderTest, SwitchInterleavedLoopsConditionals) {
  auto entry = createLabel("entry");
  auto case0 = createLabel("case0");
  auto case1 = createLabel("case1");
  auto case2 = createLabel("case2");
  auto loop0 = createLabel("loop0");
  auto cond0 = createLabel("cond0");
  auto loop1 = createLabel("loop1");
  auto exit = createLabel("exit");

  createSwitchBranch(entry, exit, {{0, case0}, {1, case1}, {2, case2}});

  // Case 0: loop then conditional
  createBranch(case0, loop0);
  createConditionalBranch(loop0, loop0, cond0); // loop
  createConditionalBranch(cond0, exit, exit);

  // Case 1: conditional then loop
  createConditionalBranch(case1, loop1, exit);
  createConditionalBranch(loop1, loop1, exit); // loop

  // Case 2: simple exit
  createBranch(case2, exit);

  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test ultra-large switch with 200 cases (stress test)
 */
TEST_F(GcnShaderTest, UltraLargeSwitch200Cases) {
  auto entry = createLabel("entry");
  std::vector<ir::Value> cases;
  for (int i = 0; i < 200; i++) {
    cases.push_back(createLabel("case" + std::to_string(i)));
  }
  auto exit = createLabel("exit");

  // Create switch with 200 cases
  std::vector<std::pair<std::uint32_t, ir::Value>> switch_cases;
  for (int i = 0; i < 200; i++) {
    switch_cases.push_back({i, cases[i]});
  }
  createSwitchBranch(entry, exit, switch_cases);

  // Each case branches to exit (simple to focus on switch complexity)
  for (auto case_label : cases) {
    createBranch(case_label, exit);
  }
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}

/**
 * Test mega switch with 500 cases (ultimate stress test)
 */
TEST_F(GcnShaderTest, MegaSwitch500Cases) {
  auto entry = createLabel("entry");
  std::vector<ir::Value> cases;
  for (int i = 0; i < 500; i++) {
    cases.push_back(createLabel("case" + std::to_string(i)));
  }
  auto exit = createLabel("exit");

  // Create switch with 500 cases
  std::vector<std::pair<std::uint32_t, ir::Value>> switch_cases;
  for (int i = 0; i < 500; i++) {
    switch_cases.push_back({i, cases[i]});
  }
  createSwitchBranch(entry, exit, switch_cases);

  // Each case branches to exit
  for (auto case_label : cases) {
    createBranch(case_label, exit);
  }
  createReturn(exit);

  EXPECT_TRUE(testStructurization());
}
