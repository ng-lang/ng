// AI-generated code; reviewed for this repository's vNext rewrite.
#include "bytecode.hpp"
#include "flowir.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "test.hpp"
#include "typecheck.hpp"

namespace bytecode = NG::bytecode;
namespace flowir = NG::flowir;
namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  [[nodiscard]] auto compile(std::string_view source) -> bytecode::Function
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    const auto typed = typecheck::TypeChecker{}.check(module);
    const auto flow = flowir::Lowerer{}.lower(module.functions.front(), typed);
    flowir::Verifier{}.verify(flow);
    return bytecode::Compiler{}.compile(flow);
  }

  [[nodiscard]] auto findEvaluate(const bytecode::Function &function, hir::ExpressionKind kind)
      -> bytecode::DecodedInstruction
  {
    const auto instructions = bytecode::Decoder{}.decode(function);
    const auto found = std::ranges::find_if(instructions,
                                            [kind](const auto &instruction)
                                            {
                                              return instruction.opcode == bytecode::Opcode::Evaluate &&
                                                     instruction.operands.size() > 1 &&
                                                     static_cast<hir::ExpressionKind>(instruction.operands[1]) == kind;
                                            });
    REQUIRE(found != instructions.end());
    return *found;
  }

  void expectVerifyError(bytecode::Function function, std::string_view message)
  {
    try
    {
      bytecode::Verifier{}.verify(function);
      FAIL("expected a verifier error");
    }
    catch (const bytecode::BytecodeError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  }

  /// Overwrites one u32 operand of the instruction whose opcode starts at
  /// `offset` (operand 0 is the first u32 after the opcode byte).
  void writeOperand(std::vector<uint8_t> &code, size_t offset, size_t operandIndex, uint32_t value)
  {
    const size_t position = offset + 1 + operandIndex * 4;
    REQUIRE(position + 4 <= code.size());
    code[position] = static_cast<uint8_t>(value);
    code[position + 1] = static_cast<uint8_t>(value >> 8);
    code[position + 2] = static_cast<uint8_t>(value >> 16);
    code[position + 3] = static_cast<uint8_t>(value >> 24);
  }
} // namespace

TEST_CASE("vNext bytecode verifier rejects mismatched block metadata tables", "[vNext][Bytecode][Verifier]")
{
  auto function = compile("fun f() { let x = 1; x }");
  function.blockParameterCounts.push_back(1);
  expectVerifyError(std::move(function), "bytecode block metadata tables do not match");

  auto locals = compile("fun f() { let x = 1; x }");
  locals.blockParameterLocals.front().push_back(999);
  expectVerifyError(std::move(locals), "bytecode block parameter locals do not match parameter count");
}

TEST_CASE("vNext bytecode verifier rejects missing or out-of-range type metadata", "[vNext][Bytecode][Verifier]")
{
  auto function = compile("fun f() { let x = 1; x }");
  function.typeDescriptors.clear();
  expectVerifyError(std::move(function), "bytecode typed function has no type descriptors");

  auto ranged = compile("fun f() { let x = 1; x }");
  ranged.valueTypes.begin()->second = typecheck::TypeId{999};
  expectVerifyError(std::move(ranged), "bytecode type metadata is out of range");
}

TEST_CASE("vNext bytecode verifier rejects malformed type descriptors", "[vNext][Bytecode][Verifier]")
{
  const auto withDescriptors = [](std::vector<typecheck::TypeDescriptor> descriptors)
  {
    bytecode::Function function;
    function.typeDescriptors = std::move(descriptors);
    return function;
  };
  typecheck::TypeDescriptor dummy;
  expectVerifyError(withDescriptors({dummy, typecheck::TypeDescriptor{.kind = static_cast<typecheck::TypeKind>(999)}}),
                    "bytecode type descriptor kind is invalid");
  expectVerifyError(withDescriptors({dummy, typecheck::TypeDescriptor{.kind = typecheck::TypeKind::DynamicArray,
                                                                      .element = typecheck::TypeId{1},
                                                                      .length = 3}}),
                    "bytecode dynamic array descriptor has a fixed length");
  expectVerifyError(withDescriptors({dummy, typecheck::TypeDescriptor{.kind = typecheck::TypeKind::FixedArray,
                                                                      .element = typecheck::TypeId{1}}}),
                    "bytecode fixed array descriptor has no length");
  expectVerifyError(withDescriptors({dummy, typecheck::TypeDescriptor{.kind = typecheck::TypeKind::DependentArray,
                                                                      .element = typecheck::TypeId{1},
                                                                      .length = 3}}),
                    "bytecode dependent array descriptor has a fixed length");
  expectVerifyError(withDescriptors({dummy, typecheck::TypeDescriptor{.kind = typecheck::TypeKind::Tuple,
                                                                      .elements = {typecheck::TypeId{1}},
                                                                      .length = 2}}),
                    "bytecode product descriptor length mismatch");
  expectVerifyError(withDescriptors({dummy, typecheck::TypeDescriptor{.kind = typecheck::TypeKind::Struct,
                                                                      .elements = {typecheck::TypeId{1}},
                                                                      .length = 1}}),
                    "bytecode nominal descriptor member count mismatch");
  expectVerifyError(withDescriptors({dummy, typecheck::TypeDescriptor{.kind = typecheck::TypeKind::Enum,
                                                                      .elements = {typecheck::TypeId{1}},
                                                                      .length = 1,
                                                                      .fieldNames = {"A"}}}),
                    "bytecode enum descriptor payload flag count mismatch");
}

TEST_CASE("vNext bytecode verifier rejects literal result type corruptions", "[vNext][Bytecode][Verifier]")
{
  const auto corrupted = [](std::string_view source, hir::ExpressionKind kind, typecheck::TypeId replacement)
  {
    auto function = compile(source);
    const auto instruction = findEvaluate(function, kind);
    function.valueTypes[instruction.operands[0]] = replacement;
    return function;
  };
  expectVerifyError(corrupted("fun f() { let x = 1.5; }", hir::ExpressionKind::FloatLiteral, typecheck::builtin::I64),
                    "bytecode float literal result is not a float type");
  expectVerifyError(
      corrupted("fun f() { let s = \"x\"; }", hir::ExpressionKind::StringLiteral, typecheck::builtin::I64),
      "bytecode string literal result is not a string type");
  expectVerifyError(
      corrupted("fun f() { let b = true; }", hir::ExpressionKind::BooleanLiteral, typecheck::builtin::I64),
      "bytecode boolean literal result is not a bool type");
  expectVerifyError(corrupted("fun f() { let a = [1]; }", hir::ExpressionKind::ArrayLiteral, typecheck::builtin::I64),
                    "bytecode array literal result is not an array type");
  expectVerifyError(
      corrupted("fun f() { let t = (1, 2); }", hir::ExpressionKind::TupleLiteral, typecheck::builtin::I64),
      "bytecode tuple literal result is not a tuple type");
  expectVerifyError(corrupted("struct S { x: i64 } fun f() { let s = S { x: 1 }; }", hir::ExpressionKind::StructLiteral,
                              typecheck::builtin::I64),
                    "bytecode struct literal result is not a struct type");
  expectVerifyError(
      corrupted("enum E { A } fun f() { let e = E.A; }", hir::ExpressionKind::EnumLiteral, typecheck::builtin::I64),
      "bytecode enum literal result is not an enum type");
}

TEST_CASE("vNext bytecode verifier rejects arithmetic and comparison corruptions", "[vNext][Bytecode][Verifier]")
{
  // Rewire the operands of a binary operation onto values of the wrong type
  // so the earlier literal/local-read checks of those values stay intact.
  const auto rewire =
      [](std::string_view source, size_t operand, hir::ExpressionKind replacementKind, std::string_view message)
  {
    INFO("rewiring " << source);
    auto function = compile(source);
    const auto instruction = findEvaluate(function, hir::ExpressionKind::Binary);
    const auto replacement = findEvaluate(function, replacementKind).operands[0];
    writeOperand(function.code, instruction.offset, 5 + operand, replacement);
    expectVerifyError(std::move(function), message);
  };
  rewire("fun f() { let s = \"x\"; let a = 2 * 3; }", 0, hir::ExpressionKind::StringLiteral,
         "bytecode integer arithmetic operand is not an integer type");
  rewire("fun f() { let s = \"x\"; let c = 1 == 2; }", 1, hir::ExpressionKind::StringLiteral,
         "bytecode equality operand type mismatch");
  rewire("fun f() { let s = \"x\"; let c = 1 < 2; }", 0, hir::ExpressionKind::StringLiteral,
         "bytecode integer comparison operand is not an integer type");
  rewire("fun f() { let b = true; let r = 1..2; }", 0, hir::ExpressionKind::BooleanLiteral,
         "bytecode range bound is not an integer type");
  {
    // `&&` lowers to control flow in the standard pipeline, so corrupt an
    // equality Binary's payload into the logical-and payload to exercise the
    // verifier's boolean operand checks.
    auto function = compile("fun f() { let c = 1 == 2; }");
    const auto instruction = findEvaluate(function, hir::ExpressionKind::Binary);
    writeOperand(function.code, instruction.offset, 2, 12);
    expectVerifyError(std::move(function), "bytecode operation operand type mismatch");
  }
}

TEST_CASE("vNext bytecode verifier rejects local and aggregate read corruptions", "[vNext][Bytecode][Verifier]")
{
  auto local = compile("fun f() { let x = 1; let y = x; }");
  {
    const auto instruction = findEvaluate(local, hir::ExpressionKind::ResolvedName);
    local.valueTypes[instruction.operands[0]] = typecheck::builtin::String;
  }
  try
  {
    bytecode::Verifier{}.verify(local);
    FAIL("expected a verifier error");
  }
  catch (const bytecode::BytecodeError &error)
  {
    REQUIRE_THAT(std::string{error.what()}, ContainsSubstring("bytecode local read type does not match result type"));
  }

  // Rewire the member receiver onto an i64 value instead of retyping the
  // receiver's own local read.
  auto member = compile("struct S { x: i64 } fun f() { let n = 1; let s = S { x: 1 }; let v = s.x; }");
  {
    const auto instruction = findEvaluate(member, hir::ExpressionKind::Member);
    const auto replacement = findEvaluate(member, hir::ExpressionKind::IntegerLiteral).operands[0];
    writeOperand(member.code, instruction.offset, 5, replacement);
  }
  expectVerifyError(std::move(member), "bytecode member receiver is not a struct");

  auto index = compile("fun f() { let n = 1; let a = [1]; let v = a[0]; }");
  {
    const auto instruction = findEvaluate(index, hir::ExpressionKind::Index);
    const auto replacement = findEvaluate(index, hir::ExpressionKind::IntegerLiteral).operands[0];
    writeOperand(index.code, instruction.offset, 5, replacement);
  }
  expectVerifyError(std::move(index), "bytecode index receiver is not an aggregate type");

  auto grouped = compile("fun f() { let b = !true; }");
  {
    const auto instruction = findEvaluate(grouped, hir::ExpressionKind::Prefix);
    grouped.valueTypes[instruction.operands[0]] = typecheck::builtin::I64;
  }
  expectVerifyError(std::move(grouped), "bytecode operation result type mismatch");
}

TEST_CASE("vNext bytecode verifier rejects malformed reference place steps", "[vNext][Bytecode][Verifier]")
{
  auto notProduct = compile("fun f() { let x = 1; let r = ref x; }");
  {
    const auto instructions = bytecode::Decoder{}.decode(notProduct);
    const auto found = std::ranges::find_if(instructions, [](const auto &instruction)
                                            { return instruction.opcode == bytecode::Opcode::MakeRef; });
    REQUIRE(found != instructions.end());
    // Insert a member step over the i64 root after the four fixed operands
    // (result, root, mutability, count): count = 2, kind 0 = member, field 0.
    writeOperand(notProduct.code, found->offset, 3, 2);
    const size_t position = found->offset + 1 + 4 * 4;
    notProduct.code.insert(notProduct.code.begin() + static_cast<std::ptrdiff_t>(position), {0, 0, 0, 0, 0, 0, 0, 0});
  }
  expectVerifyError(std::move(notProduct), "bytecode place member step receiver is not a product type");

  auto notI64 = compile("fun f() { let i = 0; let a = [1]; let r = ref a[i]; }");
  {
    const auto instructions = bytecode::Decoder{}.decode(notI64);
    const auto found = std::ranges::find_if(instructions, [](const auto &instruction)
                                            { return instruction.opcode == bytecode::Opcode::MakeRef; });
    REQUIRE(found != instructions.end());
    // Non-constant indexes become Index place steps: operand index 4 is the
    // step kind, 5 the index value id. Rewire the index value onto the array
    // literal's value so its own checks stay intact.
    const auto replacement = findEvaluate(notI64, hir::ExpressionKind::ArrayLiteral).operands[0];
    writeOperand(notI64.code, found->offset, 5, replacement);
  }
  expectVerifyError(std::move(notI64), "bytecode reference index step is not i64");

  auto malformed = compile("fun f() { let x = 1; let r = ref x; }");
  {
    const auto instructions = bytecode::Decoder{}.decode(malformed);
    const auto found = std::ranges::find_if(instructions, [](const auto &instruction)
                                            { return instruction.opcode == bytecode::Opcode::MakeRef; });
    REQUIRE(found != instructions.end());
    const size_t target = static_cast<size_t>(std::distance(instructions.begin(), found));
    // Re-encode the stream with an odd step count (one dangling step operand)
    // so decoding stays aligned and the verifier's own check fires.
    std::vector<uint8_t> code;
    for (size_t index = 0; index < instructions.size(); ++index)
    {
      code.push_back(static_cast<uint8_t>(instructions[index].opcode));
      std::vector<uint32_t> operands = instructions[index].operands;
      if (index == target)
      {
        operands = {found->operands[0], found->operands[1], found->operands[2], 1u, 0u};
      }
      for (const auto operand : operands)
      {
        for (size_t byte = 0; byte < 4; ++byte)
          code.push_back(static_cast<uint8_t>(operand >> (byte * 8)));
      }
    }
    malformed.code = std::move(code);
  }
  expectVerifyError(std::move(malformed), "bytecode reference step encoding is malformed");
}

TEST_CASE("vNext bytecode verifier rejects place assignment value mismatches", "[vNext][Bytecode][Verifier]")
{
  auto function = compile("fun f() { let mut a = [1]; let s = \"x\"; a[0] := 2; }");
  const auto instructions = bytecode::Decoder{}.decode(function);
  const auto found = std::ranges::find_if(instructions, [](const auto &instruction)
                                          { return instruction.opcode == bytecode::Opcode::AssignPlace; });
  REQUIRE(found != instructions.end());
  // Rewire the assigned value onto the string local's read result so the
  // literal's own checks stay intact.
  const auto replacement = findEvaluate(function, hir::ExpressionKind::StringLiteral).operands[0];
  writeOperand(function.code, found->offset, 2, replacement);
  expectVerifyError(std::move(function), "bytecode place assignment value type mismatch");
}
