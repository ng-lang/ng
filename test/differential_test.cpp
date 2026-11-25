// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-3: differential contract runner — ConstInterpreter vs QBE native.
//
// The same generated module is evaluated twice:
//   native path — the driver lowers it through FlowIR/QBE, executes it, and
//                 main() prints one line per probe call;
//   const path  — the test resolves/checks an identical copy of the source,
//                 then evaluates every probe call with ConstInterpreter.
// Zero output divergence across N >= 500 generated programs per run is the
// acceptance gate (09-increment-plan.md INC-3).
//
// Generator subset (deliberately total, no overflow/trap paths): non-negative
// values bounded via `% 256`, additions/subtractions/multiplications/divisions
// by nonzero literals, bounded accumulator loops, guarded recursion, and
// calls to previously declared probe functions.
//
// Self-check: a variant batch whose native side is deliberately biased (+1)
// must be detected as divergent — proving the harness can actually fail.

#include "pbt.hpp"
#include "test.hpp"

#include "const_eval.hpp"
#include "const_interp.hpp"
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;
namespace const_eval = NG::const_eval;

namespace
{
  constexpr uint32_t kProbeCount = 500;

  struct Probe
  {
    std::string name;
    int64_t argument{};
    std::string source;
  };

  struct Divergence
  {
    size_t probeIndex{};
    int64_t constPath{};
    int64_t nativePath{};
  };

  /// Non-negative, `% 256`-bounded expression over `seed`, locals, and calls
  /// to earlier probes.
  [[nodiscard]] auto generateExpression(pbt::Rng &rng, int depth, const std::vector<std::string> &locals,
                                        size_t functionIndex) -> std::string
  {
    if (depth == 0 || rng.below(0.35))
    {
      if (!locals.empty() && rng.below(0.5))
        return locals[rng.nextU64InRange(0, locals.size() - 1)];
      if (rng.below(0.4))
        return "seed";
      return std::to_string(rng.nextU64InRange(0, 255));
    }
    if (functionIndex > 0 && rng.below(0.12))
      // Literal stays small: nested calls into recursive probes must fit
      // the interpreter's MaxCallDepth budget together with the outer chain.
      return "dprobe_" + std::to_string(rng.nextU64InRange(0, functionIndex - 1)) + "(" +
             std::to_string(rng.nextU64InRange(0, 15)) + ")";

    const auto left = generateExpression(rng, depth - 1, locals, functionIndex);
    switch (rng.nextU64InRange(0, 4))
    {
    case 0:
      return "(" + left + " + " + generateExpression(rng, depth - 1, locals, functionIndex) + ") % 256";
    case 1:
      return "(" + left + " + 256 - " + generateExpression(rng, depth - 1, locals, functionIndex) + ") % 256";
    case 2:
      return "(" + left + " * " + generateExpression(rng, depth - 1, locals, functionIndex) + ") % 256";
    case 3:
      return "(" + left + " / " + std::to_string(rng.nextU64InRange(1, 9)) + ")";
    default:
      return "(" + left + " % " + std::to_string(rng.nextU64InRange(2, 9)) + ")";
    }
  }

  [[nodiscard]] auto generateProbe(pbt::Rng &rng, size_t index) -> Probe
  {
    Probe probe{.name = "dprobe_" + std::to_string(index), .argument = static_cast<int64_t>(rng.nextU64InRange(0, 48))};
    const auto kind = rng.nextU64InRange(0, 3);
    if (kind == 3)
      probe.argument = static_cast<int64_t>(rng.nextU64InRange(2, 12));
    if (kind == 0)
    {
      probe.source = "const fun " + probe.name + "(seed: i64) -> i64 => " + generateExpression(rng, 3, {}, index) + ";";
      return probe;
    }
    if (kind == 3)
    {
      // Guarded recursion with a strictly decreasing argument.
      const auto base = rng.nextU64InRange(0, 255);
      const auto stride = rng.nextU64InRange(1, 200);
      probe.source = "const fun " + probe.name +
                     "(seed: i64) -> i64 {\n"
                     "    if (seed <= 1) { return " +
                     std::to_string(base) +
                     "; }\n"
                     "    return (" +
                     probe.name + "(seed - 1) + " + std::to_string(stride) +
                     ") % 256;\n"
                     "}";
      return probe;
    }

    std::vector<std::string> locals;
    std::string statements;
    if (kind == 1)
    {
      // Let chain with a computed tail expression.
      const auto localCount = rng.nextU64InRange(1, 3);
      for (auto local = 0U; local < localCount; ++local)
      {
        auto name = "v" + std::to_string(local);
        statements += "    let mut " + name + " = " + generateExpression(rng, 3, locals, index) + ";\n";
        locals.push_back(std::move(name));
      }
      probe.source = "const fun " + probe.name + "(seed: i64) -> i64 {\n" + statements + "    return " +
                     generateExpression(rng, 3, locals, index) + ";\n}";
      return probe;
    }

    // Bounded accumulator loop mirroring example/const_fun.ng sum_to shape.
    const auto bound = rng.nextU64InRange(2, 12);
    const auto stride = rng.nextU64InRange(1, 7);
    statements += "    let mut acc = seed % 128;\n";
    statements += "    loop (it = 0) {\n";
    statements += "        acc := (acc + it + " + std::to_string(stride) + ") % 256;\n";
    statements += "        if (it == " + std::to_string(bound) + ") { return acc; }\n";
    statements += "        next (it + 1);\n";
    statements += "    }\n";
    probe.source = "const fun " + probe.name + "(seed: i64) -> i64 {\n" + statements + "}";
    return probe;
  }

  /// A simple bounded state machine over `seed`: accumulate with a stride,
  /// occasionally `continue` (skip) or `break`. Emitted once in `while`+`continue`
  /// form and once as an equivalent hand-written `loop`/`next` form so the
  /// desugar-equivalence can be verified across both evaluation paths
  /// (09-increment-plan.md INC-10).
  ///
  /// Semantics: `it` counts 0..bound-1; `continue` fires on the iteration that
  /// would make `it + 1 == skip` (skipping that iteration's accumulation),
  /// `break` fires when the accumulator reaches `stop`. The loop form mirrors
  /// this with `it + 1 == skip` and `next (it + 1)` backedges.
  [[nodiscard]] auto generateWhileStateMachine(pbt::Rng &rng, size_t index) -> std::pair<Probe, Probe>
  {
    const auto bound = rng.nextU64InRange(2, 10);
    const auto stride = rng.nextU64InRange(1, 9);
    const auto skip = rng.nextU64InRange(2, bound + 1);
    const auto stop = rng.nextU64InRange(3, 32);
    const auto base = rng.nextU64InRange(1, 255);

    const auto whileName = "wprobe_" + std::to_string(index);
    const auto loopName = "lprobe_" + std::to_string(index);

    Probe whileProbe{
        .name = whileName,
        .argument = static_cast<int64_t>(rng.nextU64InRange(0, 48)),
        .source = "const fun " + whileName + "(seed: i64) -> i64 {\n"
                  "    let mut acc = (seed + " + std::to_string(base) + ") % 256;\n"
                  "    let mut it = 0;\n"
                  "    while (it < " + std::to_string(bound) + ") {\n"
                  "        it := it + 1;\n"
                  "        if (it == " + std::to_string(skip) + ") { continue; }\n"
                  "        acc := (acc + " + std::to_string(stride) + ") % 256;\n"
                  "        if (acc == " + std::to_string(stop) + ") { break; }\n"
                  "    }\n"
                  "    return acc;\n"
                  "}"};

    // Equivalent hand-written loop form: the while header becomes the loop's
    // pre-body condition check, `continue` becomes the loop's next back-edge,
    // and `break` exits the loop (works in both loop and while per DEC-B).
    Probe loopProbe{
        .name = loopName,
        .argument = whileProbe.argument,
        .source = "const fun " + loopName + "(seed: i64) -> i64 {\n"
                  "    let mut acc = (seed + " + std::to_string(base) + ") % 256;\n"
                  "    loop (it = 0) {\n"
                  "        if (it == " + std::to_string(bound) + ") { break; }\n"
                  "        if (it + 1 == " + std::to_string(skip) + ") { next (it + 1); }\n"
                  "        acc := (acc + " + std::to_string(stride) + ") % 256;\n"
                  "        if (acc == " + std::to_string(stop) + ") { break; }\n"
                  "        next (it + 1);\n"
                  "    }\n"
                  "    return acc;\n"
                  "}"};
    return {std::move(whileProbe), std::move(loopProbe)};
  }

  [[nodiscard]] auto generateBatch(pbt::Rng &rng, uint32_t count, bool biasNativeSide)
  {
    std::vector<Probe> probes;
    probes.reserve(count);
    for (uint32_t index = 0; index < count; ++index)
      probes.push_back(generateProbe(rng, index));

    // Declared natively instead of importing prelude so the identical text
    // also resolves cleanly through the in-process Resolver (no loader).
    std::string source = "native fun print(value: i64) -> unit;\n\n";
    for (const auto &probe : probes)
      source += probe.source + "\n\n";
    source += "fun main() -> unit {\n";
    for (const auto &probe : probes)
      source += "    print(" + probe.name + "(" + std::to_string(probe.argument) + ")" +
                (biasNativeSide ? " + 1" : "") + ");\n";
    source += "}\n";
    return std::pair{std::move(source), std::move(probes)};
  }

/// Const path: evaluates one probe call against the checked module copy.
  [[nodiscard]] auto evaluateConstCall(const hir::Module &module, const std::unordered_set<uint32_t> &constFunctions,
                                       const_eval::ConstInterner &interner, const Probe &probe) -> int64_t
  {
    for (const auto &function : module.functions)
    {
      if (function.name != probe.name || !function.constFunction)
        continue;
      const_eval::ConstInterpreter interpreter{module, constFunctions, interner, {}, {}, {}};

      auto callee = std::make_unique<hir::Expression>(hir::Expression{
        .kind = hir::ExpressionKind::ResolvedName,
        .span = syntax::SourceSpan{0, 0},
        .text = probe.name,
        .resolvedName = hir::ResolvedName{.kind = hir::ResolvedNameKind::Function, .id = function.id.value}});
      auto argumentNode = std::make_unique<hir::Expression>(hir::Expression{.kind = hir::ExpressionKind::IntegerLiteral,
                                                                            .span = syntax::SourceSpan{0, 0},
                                                                            .text = std::to_string(probe.argument)});
      hir::Expression call{.kind = hir::ExpressionKind::Call, .span = syntax::SourceSpan{0, 0}, .text = probe.name};
      call.operands.push_back(std::move(callee));
      call.operands.push_back(std::move(argumentNode));
      return interner.value(interpreter.evaluateCall(call, {}, {}, syntax::SourceSpan{0, 0})).integerValue;
    }
    FAIL("probe function not found in checked module: " << probe.name);
    return 0;
  }

  /// Runs both evaluation paths over one batch and reports any divergence.
  [[nodiscard]] auto checkDivergence(uint32_t count, bool biasNativeSide) -> std::optional<Divergence>
  {
    pbt::Rng rng{pbt::currentSeed(biasNativeSide ? "diff-bias" : "diff")};
    auto [source, probes] = generateBatch(rng, count, biasNativeSide);

    const auto modulePath = std::filesystem::temp_directory_path() /
                            (biasNativeSide ? std::string{"ng_diff_bias_module.ng"} : std::string{"ng_diff_module.ng"});
    {
      std::ofstream stream(modulePath);
      stream << source;
    }

    std::ostringstream output;
    std::ostringstream errors;
    const int status = NG::runDriver({modulePath.string()}, output, errors);
    INFO(errors.str());
    REQUIRE(status == 0);
    const auto nativeValues = pbt::programOutputLines(output.str());
    REQUIRE(nativeValues.size() == count);

    const auto module = hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
    std::unordered_set<uint32_t> constFunctions;
    for (const auto &function : module.functions)
      if (function.constFunction)
        constFunctions.insert(function.id.value);

    const_eval::ConstInterner interner;
    for (size_t index = 0; index < probes.size(); ++index)
    {
      const auto constValue = evaluateConstCall(module, constFunctions, interner, probes[index]);
      const auto nativeValue = static_cast<int64_t>(std::stoll(nativeValues[index]));
      if (constValue != nativeValue)
        return Divergence{index, constValue, nativeValue};
    }
    return std::nullopt;
  }
} // namespace

TEST_CASE("INC-3 differential: 500 generated programs match across both paths", "[INC3][Differential]")
{
  const auto divergence = checkDivergence(kProbeCount, false);
  if (divergence.has_value())
  {
    FAIL("divergence at probe " << divergence->probeIndex << ": const=" << divergence->constPath
                                << " native=" << divergence->nativePath);
  }
  else
  {
    REQUIRE(!divergence.has_value());
  }
}

TEST_CASE("INC-10 while/continue desugars equivalently to a hand-written loop on both paths", "[INC10][Differential]")
{
  constexpr uint32_t kWhileCount = 300;
  pbt::Rng rng{pbt::currentSeed("diff-while")};
  std::vector<Probe> whileProbes;
  std::vector<Probe> loopProbes;
  whileProbes.reserve(kWhileCount);
  loopProbes.reserve(kWhileCount);

  std::string source = "native fun print(value: i64) -> unit;\n\n";
  for (uint32_t index = 0; index < kWhileCount; ++index)
  {
    auto [whileProbe, loopProbe] = generateWhileStateMachine(rng, index);
    whileProbes.push_back(std::move(whileProbe));
    loopProbes.push_back(std::move(loopProbe));
  }
  for (const auto &probe : whileProbes)
    source += probe.source + "\n\n";
  for (const auto &probe : loopProbes)
    source += probe.source + "\n\n";
  source += "fun main() -> unit {\n";
  for (const auto &probe : whileProbes)
    source += "    print(" + probe.name + "(" + std::to_string(probe.argument) + "));\n";
  for (const auto &probe : loopProbes)
    source += "    print(" + probe.name + "(" + std::to_string(probe.argument) + "));\n";
  source += "}\n";

  const auto modulePath = std::filesystem::temp_directory_path() / "ng_diff_while_module.ng";
  {
    std::ofstream stream(modulePath);
    stream << source;
  }

  std::ostringstream output;
  std::ostringstream errors;
  const int status = NG::runDriver({modulePath.string()}, output, errors);
  INFO(errors.str());
  REQUIRE(status == 0);
  const auto nativeValues = pbt::programOutputLines(output.str());
  REQUIRE(nativeValues.size() == kWhileCount * 2);

  const auto module = hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
  std::unordered_set<uint32_t> constFunctions;
  for (const auto &function : module.functions)
    if (function.constFunction)
      constFunctions.insert(function.id.value);

  const_eval::ConstInterner interner;
  for (size_t index = 0; index < whileProbes.size(); ++index)
  {
    const auto whileNative = static_cast<int64_t>(std::stoll(nativeValues[index]));
    const auto loopNative = static_cast<int64_t>(std::stoll(nativeValues[kWhileCount + index]));
    const auto whileConst = evaluateConstCall(module, constFunctions, interner, whileProbes[index]);
    const auto loopConst = evaluateConstCall(module, constFunctions, interner, loopProbes[index]);
    INFO("probe " << index);
    CHECK(whileNative == loopNative);
    CHECK(whileConst == loopConst);
    CHECK(whileNative == whileConst);
  }
}

TEST_CASE("INC-3 differential self-check: an injected bias must be caught", "[INC3][Differential]")
{
  const auto divergence = checkDivergence(20, true);
  REQUIRE(divergence.has_value());
  REQUIRE(divergence->nativePath == divergence->constPath + 1);
}
