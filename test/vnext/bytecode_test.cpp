// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/bytecode.hpp"
#include "vnext/flowir.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"
#include "vnext/vm.hpp"

namespace bytecode = NG::vnext::bytecode;
namespace flowir = NG::vnext::flowir;
namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;
namespace vm = NG::vnext::vm;

namespace
{
  [[nodiscard]] auto compile(std::string_view source) -> bytecode::Function
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    static_cast<void>(typecheck::TypeChecker{}.check(module));
    const auto flow = flowir::Lowerer{}.lower(module.functions.front());
    flowir::Verifier{}.verify(flow);
    return bytecode::Compiler{}.compile(flow);
  }
} // namespace

TEST_CASE("vNext bytecode compiler and decoder share loop backedge schema", "[vNext][Bytecode]")
{
  const auto function = compile("fun step(seed: i64) { loop (left = seed, right = 1) { next (right, left); } }");
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(function));

  const auto instructions = bytecode::Decoder{}.decode(function);
  const auto backedge = std::find_if(instructions.begin(), instructions.end(), [](const auto &instruction) {
    return instruction.opcode == bytecode::Opcode::LoopBackedge;
  });
  REQUIRE(backedge != instructions.end());
  REQUIRE(backedge->operands[0] == 1);
  REQUIRE(backedge->operands[1] == 2);
  REQUIRE(backedge->operands.size() == 4);
}

TEST_CASE("vNext bytecode represents tail recursion without a call target", "[vNext][Bytecode]")
{
  const auto function = compile("fun recur(value: i64) { next (value); }");
  const auto instructions = bytecode::Decoder{}.decode(function);
  REQUIRE(instructions.size() == 2);
  REQUIRE(instructions.back().opcode == bytecode::Opcode::TailRecur);
  REQUIRE(instructions.back().operands[0] == 1);
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(function));
}

TEST_CASE("vNext bytecode module compiler preserves function identities and direct calls", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun helper(value: i64) -> i64 { return value; } fun main() -> i64 { return helper(42); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  static_cast<void>(typecheck::TypeChecker{}.check(hirModule));
  std::vector<flowir::Function> flows;
  for (const auto &function : hirModule.functions) flows.push_back(flowir::Lowerer{}.lower(function));
  const auto module = bytecode::ModuleCompiler{}.compile(flows);
  REQUIRE(module.functions.size() == 2);
  REQUIRE(module.functions[0].source.value == 0);
  REQUIRE(module.functions[1].source.value == 1);
  const auto instructions = bytecode::Decoder{}.decode(module.functions[1]);
  const auto call = std::find_if(instructions.begin(), instructions.end(), [](const auto &instruction) {
    return instruction.opcode == bytecode::Opcode::Call;
  });
  REQUIRE(call != instructions.end());
  REQUIRE(call->operands[0] == 1);
  REQUIRE(call->operands[1] == 0);
  REQUIRE(call->operands[2] == 1);
}

TEST_CASE("vNext bytecode artifacts round-trip verified modules", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun helper(value: i64) -> i64 { return value + 1; } fun main() -> i64 { return helper(41); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  std::vector<flowir::Function> flows;
  for (const auto &function : hirModule.functions) flows.push_back(flowir::Lowerer{}.lower(function, typed));
  const auto module = bytecode::ModuleCompiler{}.compile(flows);
  const auto artifact = bytecode::ArtifactCodec{}.serialize(module);
  const auto restored = bytecode::ArtifactCodec{}.deserialize(artifact);
  REQUIRE(restored.functions.size() == 2);
  REQUIRE(restored.functions[0].code == module.functions[0].code);
  REQUIRE(restored.functions[1].code == module.functions[1].code);
  REQUIRE(restored.functions[1].valueTypes == module.functions[1].valueTypes);
  REQUIRE(restored.functions[1].localTypes == module.functions[1].localTypes);
  REQUIRE(vm::VM{}.run(restored, hir::DefId{1}).returnValue == 42);
}

TEST_CASE("vNext bytecode artifacts preserve fixed-array type descriptors", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun values() -> array<i64, 3> { return [1, 2, 3]; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto artifact = bytecode::ArtifactCodec{}.serialize(bytecode::Module{.functions = {function}});
  const auto restored = bytecode::ArtifactCodec{}.deserialize(artifact);
  REQUIRE(restored.functions.front().typeDescriptors == function.typeDescriptors);
  REQUIRE(restored.functions.front().typeDescriptors.at(function.valueTypes.at(3).value).kind == typecheck::TypeKind::FixedArray);
  REQUIRE(vm::VM{}.run(restored.functions.front()).returnValue->asArray().size() == 3);
}

TEST_CASE("vNext bytecode artifacts preserve string constant pools", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun greeting() -> string { return \"hello\" + \" world\"; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto artifact = bytecode::ArtifactCodec{}.serialize(bytecode::Module{.functions = {function}});
  const auto restored = bytecode::ArtifactCodec{}.deserialize(artifact);
  REQUIRE(restored.functions.front().stringConstants == function.stringConstants);
  REQUIRE(vm::VM{}.run(restored.functions.front()).returnValue == NG::vnext::Value::string("hello world"));
}

TEST_CASE("vNext bytecode artifacts reject malformed framing", "[vNext][Bytecode]")
{
  REQUIRE_THROWS_WITH(bytecode::ArtifactCodec{}.deserialize({}), "invalid bytecode artifact magic");
  REQUIRE_THROWS_WITH(bytecode::ArtifactCodec{}.deserialize({'N', 'G', 'V', 'X'}), "truncated bytecode artifact");
  REQUIRE_THROWS_WITH(bytecode::ArtifactCodec{}.deserialize({'N', 'G', 'V', 'X', 2, 0, 0, 0}),
                      "unsupported bytecode artifact version");
}

TEST_CASE("vNext bytecode verifier enforces typed register and local contracts", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun entry(flag: bool) -> i64 { let value = 1; if flag { return value; } return 0; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(function));

  function.valueTypes.erase(0);
  REQUIRE_THROWS_WITH(bytecode::Verifier{}.verify(function), "bytecode value is missing type metadata");

  function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  function.valueTypes.at(0) = typecheck::builtin::Bool;
  REQUIRE_THROWS_WITH(bytecode::Verifier{}.verify(function), "bytecode operation result type mismatch");
}

TEST_CASE("vNext bytecode verifier rejects malformed branch contracts", "[vNext][Bytecode]")
{
  bytecode::Function malformed{.code = {static_cast<uint8_t>(bytecode::Opcode::Jump), 1, 0, 0, 0, 0, 0, 0, 0},
                               .blockParameterCounts = {0},
                               .blockParameterLocals = {{}},
                               .blockOffsets = {0}};
  REQUIRE_THROWS_WITH(bytecode::Verifier{}.verify(malformed), "bytecode branch target is out of range");

  bytecode::Function truncated{.code = {static_cast<uint8_t>(bytecode::Opcode::Return), 1, 0},
                               .blockParameterCounts = {0},
                               .blockOffsets = {0}};
  REQUIRE_THROWS_WITH(bytecode::Decoder{}.decode(truncated), "truncated u32 operand");
}
