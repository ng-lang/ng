// AI-generated code; reviewed for this repository's vNext rewrite.
#include "bytecode.hpp"
#include "flowir.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"
#include "typecheck.hpp"
#include "vm.hpp"

namespace bytecode = NG::bytecode;
namespace flowir = NG::flowir;
namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;
namespace vm = NG::vm;

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
  const auto backedge = std::find_if(instructions.begin(), instructions.end(), [](const auto &instruction)
                                     { return instruction.opcode == bytecode::Opcode::LoopBackedge; });
  REQUIRE(backedge != instructions.end());
  REQUIRE(backedge->operands[0] == 1);
  REQUIRE(backedge->operands[1] == 2);
  REQUIRE(backedge->operands.size() == 4);
}

TEST_CASE("vNext bytecode encodes nominal struct member operations", "[vNext][Bytecode]")
{
  const auto syntaxUnit =
      syntax::parseSourceUnit("struct Point { x: i64, label: string } fun update() -> i64 { let mut point = Point { x: "
                              "1, label: \"p\" }; point.x := 2; return point.x; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto instructions = bytecode::Decoder{}.decode(function);
  REQUIRE(std::ranges::count_if(instructions, [](const auto &instruction)
                                { return instruction.opcode == bytecode::Opcode::AssignPlace; }) == 1);
  REQUIRE(std::ranges::count_if(instructions,
                                [](const auto &instruction)
                                {
                                  return instruction.opcode == bytecode::Opcode::Evaluate &&
                                         instruction.operands[1] ==
                                             static_cast<uint32_t>(hir::ExpressionKind::StructLiteral);
                                }) == 1);
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(function));
}

TEST_CASE("vNext bytecode encodes tuple extraction", "[vNext][Bytecode]")
{
  const auto syntaxUnit =
      syntax::parseSourceUnit("fun unpack() -> i64 { let (first, second) = (1, true); return first; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto instructions = bytecode::Decoder{}.decode(function);
  REQUIRE(std::ranges::count_if(instructions, [](const auto &instruction)
                                { return instruction.opcode == bytecode::Opcode::ExtractTuple; }) == 2);
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(function));
}

TEST_CASE("vNext bytecode encodes and verifies array index places", "[vNext][Bytecode]")
{
  const auto syntaxUnit =
      syntax::parseSourceUnit("fun update() -> i64 { let mut values = [1, 2]; values[1] := 7; return values[1]; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto instructions = bytecode::Decoder{}.decode(function);
  REQUIRE(std::ranges::count_if(instructions, [](const auto &instruction)
                                { return instruction.opcode == bytecode::Opcode::AssignPlace; }) == 1);
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(function));
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
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun helper(value: i64) -> i64 { return value; } fun main() -> i64 { return helper(42); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  static_cast<void>(typecheck::TypeChecker{}.check(hirModule));
  std::vector<flowir::Function> flows;
  for (const auto &function : hirModule.functions)
    flows.push_back(flowir::Lowerer{}.lower(function));
  const auto module = bytecode::ModuleCompiler{}.compile(flows);
  REQUIRE(module.functions.size() == 2);
  REQUIRE(module.functions[0].source.value == 0);
  REQUIRE(module.functions[1].source.value == 1);
  const auto instructions = bytecode::Decoder{}.decode(module.functions[1]);
  const auto call = std::find_if(instructions.begin(), instructions.end(),
                                 [](const auto &instruction) { return instruction.opcode == bytecode::Opcode::Call; });
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
  for (const auto &function : hirModule.functions)
    flows.push_back(flowir::Lowerer{}.lower(function, typed));
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

TEST_CASE("vNext bytecode artifacts preserve generic enum instances", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "enum Result<T, E> { Ok(value: T), Err(error: E) } fun result() -> Result<i64, string> { return Result.Ok(7); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto restored = bytecode::ArtifactCodec{}.deserialize(
      bytecode::ArtifactCodec{}.serialize(bytecode::Module{.functions = {function}}));
  const auto result = std::ranges::find_if(
      restored.functions.front().typeDescriptors, [](const auto &descriptor)
      { return descriptor.kind == typecheck::TypeKind::Enum && descriptor.typeArguments.size() == 2; });
  REQUIRE(result != restored.functions.front().typeDescriptors.end());
  REQUIRE(result->typeArguments == std::vector<typecheck::TypeId>{typecheck::builtin::I64, typecheck::builtin::String});
}

TEST_CASE("vNext bytecode artifacts preserve enum variant layouts", "[vNext][Bytecode]")
{
  const auto syntaxUnit =
      syntax::parseSourceUnit("enum Result { Ok(i64), Empty } fun result() -> Result { return Result.Ok(7); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto restored = bytecode::ArtifactCodec{}.deserialize(
      bytecode::ArtifactCodec{}.serialize(bytecode::Module{.functions = {function}}));
  REQUIRE(restored.functions.front().typeDescriptors == function.typeDescriptors);
  const auto result = vm::VM{}.run(restored.functions.front());
  REQUIRE(result.returnValue->isEnum());
  REQUIRE(result.returnValue->asEnumVariant() == 0);
  REQUIRE(result.returnValue->asEnumPayload()[0] == 7);
}

TEST_CASE("vNext bytecode artifacts preserve nominal struct layouts", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "struct Point { x: i64, label: string } fun point() -> Point { return Point { x: 7, label: \"value\" }; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto artifact = bytecode::ArtifactCodec{}.serialize(bytecode::Module{.functions = {function}});
  const auto restored = bytecode::ArtifactCodec{}.deserialize(artifact);
  REQUIRE(restored.functions.front().typeDescriptors == function.typeDescriptors);
  const auto &structure = *std::find_if(
      restored.functions.front().typeDescriptors.begin(), restored.functions.front().typeDescriptors.end(),
      [](const typecheck::TypeDescriptor &descriptor) { return descriptor.kind == typecheck::TypeKind::Struct; });
  REQUIRE(structure.kind == typecheck::TypeKind::Struct);
  REQUIRE(structure.fieldNames == std::vector<std::string>{"x", "label"});
  REQUIRE(vm::VM{}.run(restored.functions.front()).returnValue->asStruct()[0] == 7);
}

TEST_CASE("vNext bytecode artifacts preserve tuple layouts", "[vNext][Bytecode]")
{
  const auto syntaxUnit =
      syntax::parseSourceUnit("fun pair() -> tuple<i64, bool, string> { return (1, true, \"value\"); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  const auto restored = bytecode::ArtifactCodec{}.deserialize(
      bytecode::ArtifactCodec{}.serialize(bytecode::Module{.functions = {function}}));
  REQUIRE(restored.functions.front().typeDescriptors == function.typeDescriptors);
  const auto result = vm::VM{}.run(restored.functions.front());
  REQUIRE(result.returnValue->isTuple());
  REQUIRE(result.returnValue->asTuple()[2].asString() == "value");
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
  REQUIRE(restored.functions.front().typeDescriptors.at(function.valueTypes.at(3).value).kind ==
          typecheck::TypeKind::FixedArray);
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
  REQUIRE(vm::VM{}.run(restored.functions.front()).returnValue == NG::Value::string("hello world"));
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
  const auto syntaxUnit =
      syntax::parseSourceUnit("fun entry(flag: bool) -> i64 { let value = 1; if flag { return value; } return 0; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(function));

  function.valueTypes.erase(0);
  REQUIRE_THROWS_WITH(bytecode::Verifier{}.verify(function), "bytecode value is missing type metadata");

  function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  function.valueTypes.at(0) = typecheck::builtin::Bool;
  REQUIRE_THROWS_WITH(bytecode::Verifier{}.verify(function), "bytecode integer literal result is not an integer type");
}

TEST_CASE("vNext bytecode verifier rejects malformed branch contracts", "[vNext][Bytecode]")
{
  bytecode::Function malformed{.code = {static_cast<uint8_t>(bytecode::Opcode::Jump), 1, 0, 0, 0, 0, 0, 0, 0},
                               .blockParameterCounts = {0},
                               .blockParameterLocals = {{}},
                               .blockOffsets = {0}};
  REQUIRE_THROWS_WITH(bytecode::Verifier{}.verify(malformed), "bytecode branch target is out of range");

  bytecode::Function truncated{
    .code = {static_cast<uint8_t>(bytecode::Opcode::Return), 1, 0}, .blockParameterCounts = {0}, .blockOffsets = {0}};
  REQUIRE_THROWS_WITH(bytecode::Decoder{}.decode(truncated), "truncated u32 operand");
}

TEST_CASE("vNext bytecode artifacts round-trip dispatch tables", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun helper(value: i64) -> i64 { return value + 1; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto flow = flowir::Lowerer{}.lower(hirModule.functions.front(), typed);
  const uint64_t key = (static_cast<uint64_t>(3) << 32) | 7;
  const auto module = bytecode::ModuleCompiler{}.compile({flow}, {{key, {1u, 2u, 3u}}});
  const auto restored = bytecode::ArtifactCodec{}.deserialize(bytecode::ArtifactCodec{}.serialize(module));
  REQUIRE(restored.vtables.size() == 1);
  REQUIRE(restored.vtables.at(key) == std::vector<uint32_t>{1, 2, 3});
}

TEST_CASE("vNext bytecode artifacts reject duplicate type metadata ids", "[vNext][Bytecode]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun helper(value: i64) -> i64 { return value + 1; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(hirModule.functions.front(), typed));
  auto artifact = bytecode::ArtifactCodec{}.serialize(bytecode::Module{.functions = {function}});

  // Walk the framing to the first function's valueTypes map and duplicate
  // its first id so the decoder's duplicate check fires.
  const auto readU32 = [](const std::vector<uint8_t> &input, size_t &offset) -> uint32_t
  {
    REQUIRE(input.size() - offset >= 4);
    uint32_t value{};
    for (size_t index = 0; index < 4; ++index)
      value |= static_cast<uint32_t>(input[offset++]) << (index * 8);
    return value;
  };
  const auto skipStringVector = [&readU32](const std::vector<uint8_t> &input, size_t &offset)
  {
    const uint32_t count = readU32(input, offset);
    for (uint32_t index = 0; index < count; ++index)
    {
      const uint32_t size = readU32(input, offset);
      offset += size;
    }
  };
  const auto skipU32Vector = [&readU32](const std::vector<uint8_t> &input, size_t &offset)
  {
    const uint32_t count = readU32(input, offset);
    offset += static_cast<size_t>(count) * 4;
  };
  size_t offset = 4 + 4 + 4;                    // magic, version, function count
  static_cast<void>(readU32(artifact, offset)); // source
  static_cast<void>(readU32(artifact, offset)); // native flag
  offset += readU32(artifact, offset);          // name
  offset += readU32(artifact, offset);          // code
  skipStringVector(artifact, offset);
  skipU32Vector(artifact, offset); // parameter locals
  skipU32Vector(artifact, offset); // block parameter counts
  const uint32_t blockLocalCount = readU32(artifact, offset);
  for (uint32_t index = 0; index < blockLocalCount; ++index)
    skipU32Vector(artifact, offset);
  skipU32Vector(artifact, offset); // block offsets
  const uint32_t mapEntries = readU32(artifact, offset);
  REQUIRE(mapEntries >= 2);
  const size_t firstId = offset;
  static_cast<void>(readU32(artifact, offset));
  const uint32_t secondId = readU32(artifact, offset);
  for (size_t index = 0; index < 4; ++index)
    artifact[firstId + index] = static_cast<uint8_t>(secondId >> (index * 8));

  REQUIRE_THROWS_WITH(bytecode::ArtifactCodec{}.deserialize(artifact), "duplicate bytecode artifact type metadata");
}
