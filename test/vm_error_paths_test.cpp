// AI-generated code; reviewed for this repository's vNext rewrite.
// Runtime error paths: source-reachable arithmetic/heap failures plus the
// VM's defensive single-function-runner checks, exercised through compiled
// FlowIR probes. Each case also pins the diagnostic text.
#include "test.hpp"
#include "bytecode.hpp"
#include "driver.hpp"
#include "flowir.hpp"
#include "hir.hpp"
#include "vm.hpp"

#include <sstream>

namespace bytecode = NG::bytecode;
namespace flowir = NG::flowir;
namespace hir = NG::hir;
namespace typecheck = NG::typecheck;
namespace vm = NG::vm;

namespace
{
  auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  [[nodiscard]] auto probeFunction(std::vector<flowir::Instruction> instructions, std::optional<flowir::Terminator> terminator)
      -> bytecode::Function
  {
    flowir::Function function{.source = hir::DefId{0}, .name = "probe"};
    function.entry = flowir::BlockId{0};
    flowir::Block block{.id = flowir::BlockId{0}};
    block.instructions = std::move(instructions);
    block.terminator = terminator.has_value()
                           ? std::move(terminator)
                           : flowir::Terminator{.kind = flowir::TerminatorKind::Return, .arguments = {}};
    function.blocks.push_back(std::move(block));
    return bytecode::Compiler{}.compile(function);
  }
} // namespace

TEST_CASE("vNext VM runtime arithmetic errors carry exact diagnostics", "[vNext][VM][Errors]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() { let zero = 0; let r = 5 / zero; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("integer division by zero"));

  REQUIRE(run({"--source", "fun main() { let zero = 0; let r = 5 % zero; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("integer remainder by zero"));

  REQUIRE(run({"--source", "fun main() { let min = -9223372036854775807 - 1; let r = min / -1; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("integer division overflow"));

  REQUIRE(run({"--source", "fun main() { let min = -9223372036854775807 - 1; let r = min % -1; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("integer remainder overflow"));

  REQUIRE(run({"--source", "fun main() { let min = -9223372036854775807 - 1; let r = -min; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("integer negation overflow"));

  REQUIRE(run({"--source", "fun main() { let r = 1 << 64; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("integer shift count is out of range"));

  REQUIRE(run({"--source", "fun main() { let r = 1 >> -1; }"}, output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("integer shift count is out of range"));
}

TEST_CASE("vNext memory handle misuse reports invalid handles", "[vNext][VM][Errors]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "import memory; fun main() { let mut handle = allocate(7); release(handle); let v = load(handle); }"},
              output, errors) == 1);
  REQUIRE_THAT(errors, ContainsSubstring("invalid heap handle"));
}

TEST_CASE("vNext VM single-function runner rejects argument count mismatches", "[vNext][VM][Errors]")
{
  const auto function = probeFunction({}, std::nullopt);
  REQUIRE_THROWS_WITH(vm::VM{}.run(function, std::vector<int64_t>{1}), "bytecode function argument count mismatch");
}

TEST_CASE("vNext VM single-function runner rejects direct and trait calls", "[vNext][VM][Errors]")
{
  auto callProbe = probeFunction(
      {flowir::Instruction{.kind = flowir::InstructionKind::Evaluate,
                           .result = flowir::ValueId{0},
                           .expressionKind = hir::ExpressionKind::Call,
                           .callTarget = hir::DefId{0},
                           .operands = {}}},
      std::nullopt);
  callProbe.valueTypes.emplace(0, typecheck::builtin::I64);
  callProbe.typeDescriptors.push_back(typecheck::TypeDescriptor{.kind = typecheck::TypeKind::Builtin, .name = "unused"});
  callProbe.typeDescriptors.push_back(typecheck::TypeDescriptor{.kind = typecheck::TypeKind::Builtin, .name = "i64"});
  REQUIRE_THROWS_WITH(vm::VM{}.run(callProbe), "direct calls require a bytecode module");

  auto traitProbe = probeFunction(
      {flowir::Instruction{.kind = flowir::InstructionKind::CallTrait,
                           .result = flowir::ValueId{0},
                           .payload = 0,
                           .operands = {flowir::ValueId{1}}}},
      std::nullopt);
  REQUIRE_THROWS_WITH(vm::VM{}.run(traitProbe), "trait dispatch requires a bytecode module");
}

TEST_CASE("vNext VM rejects block offsets that miss instruction boundaries", "[vNext][VM][Errors]")
{
  flowir::Function function{.source = hir::DefId{0}, .name = "probe"};
  function.entry = flowir::BlockId{0};
  flowir::Block entryBlock{.id = flowir::BlockId{0}};
  entryBlock.terminator = flowir::Terminator{.kind = flowir::TerminatorKind::Jump, .targets = {flowir::BlockId{1}}};
  function.blocks.push_back(std::move(entryBlock));
  flowir::Block tailBlock{.id = flowir::BlockId{1}};
  tailBlock.terminator = flowir::Terminator{.kind = flowir::TerminatorKind::Return, .arguments = {}};
  function.blocks.push_back(std::move(tailBlock));
  auto compiled = bytecode::Compiler{}.compile(function);
  compiled.blockOffsets[1] = 999;   // no instruction starts at offset 999
  REQUIRE_THROWS_WITH(vm::VM{}.run(compiled), "bytecode block offset is not an instruction boundary");
}
