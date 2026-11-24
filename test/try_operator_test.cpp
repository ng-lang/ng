// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-13 (D-007): the `?` try operator is desugared on the HIR side
// (src/hir.cpp Resolver::desugarTry) into a scrutinee let + switch whose Err
// arm early-returns `Result.Err($err)`, riding the existing drop-edge tables
// and FlowIR switch/return lowering unchanged.
//
// * Differential PBT: `let y = g(x)?; ...` is behaviorally identical to the
//   hand-written switch expansion (generated call chains, dual path
//   comparison over property-generated inputs).
// * Fixed behavior: Ok payload binding (incl. annotation / `let mut` /
//   function-body tail), chain propagation, bare-statement form, `?` inside
//   nested if branches, `return e?`, and the map-spread filter marker
//   (`f(xs)?...`) which remains unchanged.
// * Negative contracts: non-Result return, unit return, closure bodies, and
//   destructuring lets.

#include "test.hpp"

#include "driver.hpp"
#include "pbt.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <sstream>
#include <string>
#include <vector>

namespace
{
  constexpr int64_t kFloor = -6;
  constexpr int64_t kCeil = 6;

  [[nodiscard]] auto boundItem(pbt::Rng &rng) -> int64_t
  {
    if (rng.below(0.2))
      return kFloor;
    if (rng.below(0.2))
      return kCeil;
    if (rng.below(0.2))
      return 0;
    return static_cast<int64_t>(rng.nextU64InRange(0, static_cast<uint64_t>(kCeil - kFloor))) + kFloor;
  }

  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--source", source}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  [[nodiscard]] auto requireRejected(std::string_view source, std::string_view message) -> void
  {
    std::string output;
    std::string errors;
    const int status = run(source, output, errors);
    REQUIRE(status != 0);
    INFO("errors: " << errors);
    REQUIRE_THAT(errors, Catch::Matchers::ContainsSubstring(std::string{message}));
  }
} // namespace

TEST_CASE("INC-13 `?` differential equals the hand-written switch expansion (PBT)", "[INC13][TryOperator][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("try-operator");
  constexpr uint32_t kTrials = 60;

  const char *kPreamble = R"(import prelude;
fun step1(x: i64) -> Result<i64, string> {
    if (x < 0) { let e: Result<i64, string> = Result.Err("neg"); return e; }
    let r: Result<i64, string> = Result.Ok(x + 1);
    return r;
}
fun step2(x: i64) -> Result<i64, string> {
    if (x % 2 == 0) { let e: Result<i64, string> = Result.Err("even"); return e; }
    let r: Result<i64, string> = Result.Ok(x * 3);
    return r;
}
fun step3(x: i64) -> Result<i64, string> {
    if (x > 20) { let e: Result<i64, string> = Result.Err("big"); return e; }
    let r: Result<i64, string> = Result.Ok(x - 1);
    return r;
}
fun outcome(r: Result<i64, string>) -> i64 {
    switch (r) {
        case Ok(v) { return v; }
        case Err(e) { if (e == "neg") { return -1; } if (e == "even") { return -2; } return -3; }
    }
}
fun chainQ2(v: i64) -> Result<i64, string> {
    let a = step1(v)?;
    let b = step2(a)?;
    let r: Result<i64, string> = Result.Ok(b + 2);
    return r;
}
fun chainS2(v: i64) -> Result<i64, string> {
    let a0 = step1(v);
    switch (a0) {
        case Ok(a) {
            let b0 = step2(a);
            switch (b0) {
                case Ok(b) { let r: Result<i64, string> = Result.Ok(b + 2); return r; }
                case Err(e) { let r: Result<i64, string> = Result.Err(e); return r; }
            }
        }
        case Err(e) { let r: Result<i64, string> = Result.Err(e); return r; }
    }
}
fun chainQ3(v: i64) -> Result<i64, string> {
    let a = step1(v)?;
    let b = step2(a)?;
    let c = step3(b)?;
    let r: Result<i64, string> = Result.Ok(c + 1);
    return r;
}
fun chainS3(v: i64) -> Result<i64, string> {
    let a0 = step1(v);
    switch (a0) {
        case Ok(a) {
            let b0 = step2(a);
            switch (b0) {
                case Ok(b) {
                    let c0 = step3(b);
                    switch (c0) {
                        case Ok(c) { let r: Result<i64, string> = Result.Ok(c + 1); return r; }
                        case Err(e) { let r: Result<i64, string> = Result.Err(e); return r; }
                    }
                }
                case Err(e) { let r: Result<i64, string> = Result.Err(e); return r; }
            }
        }
        case Err(e) { let r: Result<i64, string> = Result.Err(e); return r; }
    }
}
fun chainDropQ(v: i64) -> Result<i64, string> {
    let tag = "tag";
    let a = step1(v)?;
    let r: Result<i64, string> = Result.Ok(a + 1);
    return r;
}
fun chainDropS(v: i64) -> Result<i64, string> {
    let tag = "tag";
    let a0 = step1(v);
    switch (a0) {
        case Ok(a) { let r: Result<i64, string> = Result.Ok(a + 1); return r; }
        case Err(e) { let r: Result<i64, string> = Result.Err(e); return r; }
    }
})";

  std::vector<pbt::BatchProbe> probes;
  pbt::Rng rng(seed);

  for (uint32_t trial = 0; trial < kTrials; ++trial)
  {
    const int64_t v = boundItem(rng);
    const auto lit = pbt::decimalLiteral(v);
    probes.push_back(pbt::BatchProbe{.source = std::format("    let c2q{0} = outcome(chainQ2({1}));\n"
                                                           "    let c2s{0} = outcome(chainS2({1}));\n"
                                                           "    print(c2q{0} == c2s{0});",
                                                           trial, lit),
                                     .label = "2-step chain differential"});
    probes.push_back(pbt::BatchProbe{.source = std::format("    let c3q{0} = outcome(chainQ3({1}));\n"
                                                           "    let c3s{0} = outcome(chainS3({1}));\n"
                                                           "    print(c3q{0} == c3s{0});",
                                                           trial, lit),
                                     .label = "3-step chain differential"});
    probes.push_back(pbt::BatchProbe{.source = std::format("    let cdq{0} = outcome(chainDropQ({1}));\n"
                                                           "    let cds{0} = outcome(chainDropS({1}));\n"
                                                           "    print(cdq{0} == cds{0});",
                                                           trial, lit),
                                     .label = "drop-edge chain differential"});
  }

  const auto failure = pbt::runBatchProbes("try-operator", kPreamble, probes);
  INFO("seed=" << seed);
  REQUIRE(failure == std::nullopt);
}

TEST_CASE("INC-13 `?` unwraps Ok and early-returns Err", "[INC13][TryOperator]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(import prelude;
fun g(x: i64) -> Result<i64, string> {
    if (x == 0) { let e: Result<i64, string> = Result.Err("zero"); return e; }
    let r: Result<i64, string> = Result.Ok(x * 2);
    return r;
}
fun f(x: i64) -> Result<i64, string> {
    let y = g(x)?;
    let r2: Result<i64, string> = Result.Ok(y + 1);
    return r2;
}
fun main() {
    switch (f(3)) { case Ok(v) { print(v == 7); } case Err(e) { print(false); } }
    switch (f(0)) { case Ok(v) { print(false); } case Err(e) { print(e == "zero"); } }
})",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue"));
}

TEST_CASE("INC-13 `return e?` unwraps Ok and propagates Err", "[INC13][TryOperator]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(import prelude;
fun g(x: i64) -> Result<i64, string> {
    if (x == 0) { let e: Result<i64, string> = Result.Err("zero"); return e; }
    let r: Result<i64, string> = Result.Ok(x * 2);
    return r;
}
fun okPath(x: i64) -> Result<i64, string> {
    return g(x)?;
}
fun errPath(x: i64) -> Result<i64, string> {
    return g(x)?;
}
fun main() {
    switch (okPath(3)) { case Ok(v) { print(v == 6); } case Err(e) { print(false); } }
    switch (errPath(0)) { case Ok(v) { print(false); } case Err(e) { print(e == "zero"); } }
})",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue"));
}

TEST_CASE("INC-13 `?` binds into annotation, let mut, and function-body tail", "[INC13][TryOperator]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(import prelude;
fun step1(x: i64) -> Result<i64, string> {
    if (x < 0) { let e: Result<i64, string> = Result.Err("neg"); return e; }
    let r: Result<i64, string> = Result.Ok(x + 1);
    return r;
}
fun tailForm(x: i64) -> Result<i64, string> {
    let y = step1(x)?;
    let r: Result<i64, string> = Result.Ok(y * 2);
    r
}
fun mutAnnot(x: i64) -> Result<i64, string> {
    let mut y: i64 = step1(x)?;
    y := y + 100;
    let r: Result<i64, string> = Result.Ok(y);
    return r;
}
fun inIf(x: i64) -> Result<i64, string> {
    if (x > 0) {
        let y = step1(x)?;
        let r: Result<i64, string> = Result.Ok(y);
        return r;
    }
    let r: Result<i64, string> = Result.Ok(-1);
    return r;
}
fun main() {
    switch (tailForm(2)) { case Ok(v) { print(v == 6); } case Err(e) { print(false); } }
    switch (tailForm(-4)) { case Ok(v) { print(false); } case Err(e) { print(e == "neg"); } }
    switch (mutAnnot(5)) { case Ok(v) { print(v == 106); } case Err(e) { print(false); } }
    switch (inIf(3)) { case Ok(v) { print(v == 4); } case Err(e) { print(false); } }
    switch (inIf(-1)) { case Ok(v) { print(v == -1); } case Err(e) { print(false); } }
})",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue\ntrue\ntrue\ntrue"));
}

TEST_CASE("INC-13 bare-statement `?` discards the payload and propagates Err", "[INC13][TryOperator]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(import prelude;
fun step1(x: i64) -> Result<i64, string> {
    if (x < 0) { let e: Result<i64, string> = Result.Err("neg"); return e; }
    let r: Result<i64, string> = Result.Ok(x + 1);
    return r;
}
fun bare(x: i64) -> Result<i64, string> {
    step1(x)?;
    let r: Result<i64, string> = Result.Ok(7);
    return r;
}
fun main() {
    switch (bare(2)) { case Ok(v) { print(v == 7); } case Err(e) { print(false); } }
    switch (bare(-1)) { case Ok(v) { print(false); } case Err(e) { print(e == "neg"); } }
})",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue"));
}

TEST_CASE("INC-13 the map-spread filter marker `f(xs)?...` is unchanged", "[INC13][TryOperator]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(import prelude;
fun even(x: i64) -> bool { return x % 2 == 0; }
fun main() -> i64 {
    let xs = [1, 2, 3, 4];
    let evens = [even(xs)?...];
    return evens[0] + evens[1];
})",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("native main exited with code 6"));
}

TEST_CASE("INC-13 mid-expression `?` unwraps in evaluation order", "[INC13][TryOperator]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(import prelude;
fun g(x: i64) -> Result<i64, string> {
    if (x == 0) { let e: Result<i64, string> = Result.Err("zero"); return e; }
    let r: Result<i64, string> = Result.Ok(x);
    return r;
}
fun add(a: i64, b: i64) -> Result<i64, string> {
    let v = g(a)? + g(b)?;
    let r: Result<i64, string> = Result.Ok(v);
    return r;
}
fun main() {
    switch (add(2, 3)) { case Ok(v) { print(v == 5); } case Err(e) { print(false); } }
    switch (add(0, 3)) { case Ok(v) { print(false); } case Err(e) { print(e == "zero"); } }
    switch (add(2, 0)) { case Ok(v) { print(false); } case Err(e) { print(e == "zero"); } }
})",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring("true\ntrue\ntrue"));
}

TEST_CASE("INC-13 negative contracts", "[INC13][TryOperator][Negative]")
{
  const char *kResultHelper = R"(import prelude;
fun g(x: i64) -> Result<i64, string> {
    let r: Result<i64, string> = Result.Ok(x);
    return r;
}
)";

  requireRejected(kResultHelper + std::string{"fun f() -> i64 { let y = g(1)?; return y; }"},
                  "`?` requires the enclosing function to return `Result` (it returns `i64`)");
  requireRejected(kResultHelper + std::string{"fun main() { let y = g(1)?; print(y == 1); }"},
                  "`?` requires the enclosing function to return `Result` (it returns `unit`)");
  requireRejected(kResultHelper + std::string{"fun f() -> Result<i64, string> { let h = |a: i64| { let z = g(a)?; z }; "
                                              "let r: Result<i64, string> = Result.Ok(0); return r; }"},
                  "`?` is not supported inside a closure body");
  requireRejected(kResultHelper + std::string{"fun f() -> Result<i64, string> { let (p, q) = g(0)?; "
                                              "let r: Result<i64, string> = Result.Ok(0); return r; }"},
                  "`?` with a destructuring `let` is not supported");
}