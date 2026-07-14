// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/flowir.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"

namespace flowir = NG::vnext::flowir;
namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;

namespace
{
  [[nodiscard]] auto lower(std::string_view source) -> flowir::Function
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    static_cast<void>(typecheck::TypeChecker{}.check(module));
    return flowir::Lowerer{}.lower(module.functions.front());
  }

  [[nodiscard]] auto countTerminators(const flowir::Function &function, flowir::TerminatorKind kind) -> size_t
  {
    size_t count{};
    for (const auto &block : function.blocks)
    {
      if (block.terminator.has_value() && block.terminator->kind == kind)
      {
        ++count;
      }
    }
    return count;
  }
} // namespace

TEST_CASE("vNext FlowIR lowers loop next to a backedge with simultaneous arguments", "[vNext][FlowIR]")
{
  const auto function = lower("fun step(seed: i64) { loop (left = seed, right = 1) { next (right, left); } }");
  REQUIRE(function.entry.value == 0);
  REQUIRE(function.blocks.size() == 4);
  REQUIRE(countTerminators(function, flowir::TerminatorKind::LoopBackedge) == 1);

  const auto &entry = function.blocks[0];
  REQUIRE(entry.terminator->kind == flowir::TerminatorKind::Jump);
  REQUIRE(entry.terminator->arguments.size() == 2);

  const auto &backedge = function.blocks[2];
  REQUIRE(backedge.terminator->kind == flowir::TerminatorKind::LoopBackedge);
  REQUIRE(backedge.terminator->targets.size() == 1);
  REQUIRE(backedge.terminator->targets[0].value == 1);
  REQUIRE(backedge.terminator->arguments.size() == 2);
  REQUIRE_NOTHROW(flowir::Verifier{}.verify(function));
}

TEST_CASE("vNext FlowIR lowers function next to frame-reusing tail recursion terminator", "[vNext][FlowIR]")
{
  const auto function = lower("fun recur(value: i64) { next (value); }");
  REQUIRE(function.blocks.size() == 1);
  REQUIRE(function.blocks[0].terminator->kind == flowir::TerminatorKind::TailRecur);
  REQUIRE(function.blocks[0].terminator->targets.empty());
  REQUIRE(function.blocks[0].terminator->arguments.size() == 1);
  REQUIRE_NOTHROW(flowir::Verifier{}.verify(function));
}

TEST_CASE("vNext FlowIR creates branch and join blocks without legacy control transfer", "[vNext][FlowIR]")
{
  const auto function = lower("fun choose(flag: bool) { if flag { return; } else { return; } }");
  REQUIRE(function.blocks.size() == 4);
  REQUIRE(function.blocks[0].terminator->kind == flowir::TerminatorKind::Branch);
  REQUIRE(function.blocks[0].terminator->targets.size() == 2);
  REQUIRE(countTerminators(function, flowir::TerminatorKind::Return) == 3);
  REQUIRE_NOTHROW(flowir::Verifier{}.verify(function));
}

TEST_CASE("vNext FlowIR verifier rejects malformed targets and block arguments", "[vNext][FlowIR]")
{
  flowir::Function invalidTarget{.source = hir::DefId{0},
                                 .entry = flowir::BlockId{0},
                                 .blocks = {flowir::Block{.id = flowir::BlockId{0},
                                                          .terminator = flowir::Terminator{.kind = flowir::TerminatorKind::Jump,
                                                                                           .targets = {flowir::BlockId{1}},
                                                                                           .arguments = {}}}}};
  REQUIRE_THROWS_WITH(flowir::Verifier{}.verify(invalidTarget), "FlowIR terminator target is out of range");

  flowir::Function invalidArguments{
      .source = hir::DefId{0},
      .entry = flowir::BlockId{0},
      .blocks = {flowir::Block{.id = flowir::BlockId{0},
                               .terminator = flowir::Terminator{.kind = flowir::TerminatorKind::Jump,
                                                                .targets = {flowir::BlockId{1}},
                                                                .arguments = {}}},
                 flowir::Block{.id = flowir::BlockId{1}, .parameterCount = 1,
                               .terminator = flowir::Terminator{.kind = flowir::TerminatorKind::Return}}}};
  REQUIRE_THROWS_WITH(flowir::Verifier{}.verify(invalidArguments),
                      "FlowIR terminator argument count does not match target block parameters");
}
