// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-12: prelude Option<T> / Result<T, E> combinators validated against the
// functor and monoid-style laws over property-generated values.
//
// The combinators are pure library code (lib/std/prelude.ng) built on the
// INC-14 closure machinery. Because generated law checks must compare Option
// / Result values structurally, and the language has no derived Eq for generic
// enums, each law is expressed as a nested switch over both operands reducing
// to a concrete i64 equality. Every probe is self-contained (unique names per
// trial index) so a whole batch compiles and runs as one module via the INC-1
// runBatchProbes harness.
//
// Laws checked per trial (each prints `true`):
//   * Option identity: optMap(o, |x| x) == o
//   * Option composition on Some: optMap(optMap(o, f), g) == optMap(o, g.f)
//   * Option composition on None: optMap(optMap(None, f), g) == None
//   * Option orElse default on None: optOrElse(None, || Some(d)) == Some(d)
//   * Option orElse passthrough on Some: optOrElse(Some(v), || d) == Some(v)
//   * Result composition on Ok: resultMap(resultMap(r, f), g) == resultMap(r, g.f)
//   * Result composition on Err: resultMap(resultMap(Err(e), f), g) == resultMap(Err(e), g.f)
//
// The pure transforms are a fixed sub-domain so products/additions cannot
// trap the checked arithmetic.

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <vector>

#include "pbt.hpp"

namespace
{
  constexpr int64_t kSeedFloor = -6;
  constexpr int64_t kSeedCeil = 6;

  /// A bounded interleaving of kSeedFloor..kSeedCeil with boundary weighting.
  [[nodiscard]] auto boundItem(pbt::Rng &rng) -> int64_t
  {
    if (rng.below(0.2))
      return kSeedFloor;
    if (rng.below(0.2))
      return kSeedCeil;
    if (rng.below(0.2))
      return 0;
    return static_cast<int64_t>(rng.nextU64InRange(0, static_cast<uint64_t>(kSeedCeil - kSeedFloor))) + kSeedFloor;
  }

  /// A pure transform used as the combinator argument; the result stays in a
  /// bounded sub-domain (|f(x)| <= |x| + 1) so nested composition cannot trap.
  [[nodiscard]] auto transformCode(uint32_t index) -> std::string
  {
    return std::format("(2 * x{0} - 3)", index);
  }
} // namespace

TEST_CASE("INC-12 prelude Option/Result combinator laws", "[INC12][Option][Result][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("option-result");
  constexpr uint32_t kTrials = 60;

  const char *kPreamble = R"(import prelude;
fun projOpt(x: i64) -> i64 { return 3 * x + 4; }
fun projOpt2(x: i64) -> i64 { return 5 * x - 2; }
fun projOk(x: i64) -> i64 { return 7 * x + 1; }
fun projOk2(x: i64) -> i64 { return 2 * x - 9; })";

  std::vector<pbt::BatchProbe> probes;
  pbt::Rng rng(seed);

  for (uint32_t trial = 0; trial < kTrials; ++trial)
  {
    const int64_t v = boundItem(rng);
    const auto lit = pbt::decimalLiteral(v);
    const auto f1 = transformCode(trial);

    // Option identity: optMap(o, |x| x) == o. Built over both a Some and a
    // None value so the empty case is exercised every trial.
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let vi{0}: i64 = {1};\n"
            "    let oid{0}: Option<i64> = Option.Some({1});\n"
            "    let oidr{0} = optMap(oid{0}, |x: i64| x);\n"
            "    let mut pid{0} = false;\n"
            "    switch (oidr{0}) {{\n"
            "        case Some(rv{0}) {{ pid{0} := rv{0} == vi{0}; }}\n"
            "        case None {{}}\n"
            "    }}\n"
            "    print(pid{0});",
            trial, lit),
        .expect = "true",
        .label = std::format("Option identity Some({})", lit),
    });
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let rn{0}: Option<i64> = Option.None;\n"
            "    let oidn{0} = optMap(rn{0}, |x: i64| x);\n"
            "    let mut pidn{0} = false;\n"
            "    switch (oidn{0}) {{\n"
            "        case Some(rn2{0}) {{}}\n"
            "        case None {{ pidn{0} := true; }}\n"
            "    }}\n"
            "    print(pidn{0});",
            trial),
        .expect = "true",
        .label = "Option identity None",
    });

    // Option composition on a raw Option value: optMap(optMap(o, f), g) must
    // equal optMap(o, g.f). Two separately-annotated sources keep the moves
    // independent (values are affine; no clone of an enum locally available).
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let vs{0}: i64 = {1};\n"
            "    let s{0}a: Option<i64> = Option.Some({1});\n"
            "    let s{0}b: Option<i64> = Option.Some({1});\n"
            "    let compA{0} = optMap(optMap(s{0}a, |x: i64| projOpt(x)), |x: i64| projOpt2(x));\n"
            "    let compB{0} = optMap(s{0}b, |x: i64| projOpt2(projOpt(x)));\n"
            "    let mut pc{0} = false;\n"
            "    switch (compA{0}) {{\n"
            "        case Some(av{0}) {{\n"
            "            switch (compB{0}) {{\n"
            "                case Some(bv{0}) {{ pc{0} := av{0} == bv{0}; }}\n"
            "                case None {{}}\n"
            "            }}\n"
            "        }}\n"
            "        case None {{\n"
            "            switch (compB{0}) {{\n"
            "                case Some(bv{0}) {{}}\n"
            "                case None {{ pc{0} := true; }}\n"
            "            }}\n"
            "        }}\n"
            "    }}\n"
            "    print(pc{0});",
            trial, lit),
        .expect = "true",
        .label = std::format("Option composition on Some({})", lit),
    });
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let s{0}n: Option<i64> = Option.None;\n"
            "    let compN{0} = optMap(optMap(s{0}n, |x: i64| projOpt(x)), |x: i64| projOpt2(x));\n"
            "    let mut pcn{0} = false;\n"
            "    switch (compN{0}) {{\n"
            "        case Some(cnv{0}) {{}}\n"
            "        case None {{ pcn{0} := true; }}\n"
            "    }}\n"
            "    print(pcn{0});",
            trial),
        .expect = "true",
        .label = "Option composition on None",
    });

    // Option orElse: on None the default wins; on Some the value is kept even
    // when the default differs.
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let d{0}t: i64 = {1};\n"
            "    let orn{0}: Option<i64> = Option.None;\n"
            "    let orr{0} = optOrElse(orn{0}, || {{ let fb{0}: Option<i64> = Option.Some({1}); fb{0} }});\n"
            "    let mut po{0} = false;\n"
            "    switch (orr{0}) {{\n"
            "        case Some(ov{0}) {{ po{0} := ov{0} == d{0}t; }}\n"
            "        case None {{ po{0} := false; }}\n"
            "    }}\n"
            "    print(po{0});",
            trial, lit),
        .expect = "true",
        .label = std::format("Option orElse None -> default {}", lit),
    });
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let dev{0}: i64 = {1};\n"
            "    let os{0}: Option<i64> = Option.Some({1});\n"
            "    let err{0} = optOrElse(os{0}, || {{ let fd{0}: Option<i64> = Option.Some(9999); fd{0} }});\n"
            "    let mut pos{0} = false;\n"
            "    switch (err{0}) {{\n"
            "        case Some(ev{0}) {{ pos{0} := ev{0} == dev{0}; }}\n"
            "        case None {{ pos{0} := false; }}\n"
            "    }}\n"
            "    print(pos{0});",
            trial, lit),
        .expect = "true",
        .label = std::format("Option orElse Some({}) keeps value", lit),
    });

    // Result composition on Ok and Err.
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let rv{0}: i64 = {1};\n"
            "    let ok{0}a: Result<i64, i64> = Result.Ok({1});\n"
            "    let ok{0}b: Result<i64, i64> = Result.Ok({1});\n"
            "    let rcompA{0} = resultMap(resultMap(ok{0}a, |x: i64| projOk(x)), |x: i64| projOk2(x));\n"
            "    let rcompB{0} = resultMap(ok{0}b, |x: i64| projOk2(projOk(x)));\n"
            "    let mut pr{0} = false;\n"
            "    switch (rcompA{0}) {{\n"
            "        case Ok(rav{0}) {{\n"
            "            switch (rcompB{0}) {{\n"
            "                case Ok(rbv{0}) {{ pr{0} := rav{0} == rbv{0}; }}\n"
            "                case Err(re{0}) {{}}\n"
            "            }}\n"
            "        }}\n"
            "        case Err(rae{0}) {{\n"
            "            switch (rcompB{0}) {{\n"
            "                case Ok(rbv{0}) {{}}\n"
            "                case Err(rbe{0}) {{ pr{0} := rae{0} == rbe{0}; }}\n"
            "            }}\n"
            "        }}\n"
            "    }}\n"
            "    print(pr{0});",
            trial, lit),
        .expect = "true",
        .label = std::format("Result composition on Ok({})", lit),
    });
    probes.push_back(pbt::BatchProbe{
        .source = std::format(
            "    let ee{0}: i64 = {1};\n"
            "    let en{0}a: Result<i64, i64> = Result.Err({1});\n"
            "    let en{0}b: Result<i64, i64> = Result.Err({1});\n"
            "    let enA{0} = resultMap(resultMap(en{0}a, |x: i64| projOk(x)), |x: i64| projOk2(x));\n"
            "    let enB{0} = resultMap(en{0}b, |x: i64| projOk2(projOk(x)));\n"
            "    let mut pen{0} = false;\n"
            "    switch (enA{0}) {{\n"
            "        case Ok(eav{0}) {{}}\n"
            "        case Err(eae{0}) {{\n"
            "            switch (enB{0}) {{\n"
            "                case Ok(ebv{0}) {{}}\n"
            "                case Err(ebe{0}) {{ pen{0} := eae{0} == ebe{0}; }}\n"
            "            }}\n"
            "        }}\n"
            "    }}\n"
            "    print(pen{0});",
            trial, lit),
        .expect = "true",
        .label = std::format("Result composition on Err({})", lit),
    });
  }

  const auto failure = pbt::runBatchProbes("s_inc12_laws", kPreamble, probes);
  if (failure.has_value())
    FAIL("replay with PBT_SEED=" << seed << ": " << *failure);
  REQUIRE(failure == std::nullopt);
  SUCCEED();
}
