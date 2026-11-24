// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-33: `case (a, b)` destructures a tuple scrutinee by position.
// Distinct from INC-23 union type patterns `case (name: T)`.

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
  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--source", source}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  auto requireRejected(std::string_view source, std::string_view message) -> void
  {
    std::string output;
    std::string errors;
    const int status = run(source, output, errors);
    REQUIRE(status != 0);
    INFO("errors: " << errors);
    REQUIRE_THAT(errors, ContainsSubstring(std::string{message}));
  }
} // namespace

TEST_CASE("INC-33 tuple case patterns bind by position", "[INC33][TupleSwitch]")
{
  std::string output;
  std::string errors;
  const int status = run(R"(import prelude;
fun add(pair: tuple<i64, i64>) -> i64 {
    switch (pair) {
        case (a, b) { return a + b; }
    }
}
fun main() {
    print(add((2, 3)) == 5);
    switch ((1, "x")) { case (n, s) { print(n == 1); print(s == "x"); } }
})",
                         output, errors);
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, ContainsSubstring("true\ntrue\ntrue"));
}

TEST_CASE("INC-33 tuple case patterns equal index destructuring (PBT)", "[INC33][TupleSwitch][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("tuple-switch");
  pbt::Rng rng(seed);
  constexpr uint32_t kTrials = 40;
  std::vector<pbt::BatchProbe> probes;
  probes.reserve(kTrials);
  for (uint32_t trial = 0; trial < kTrials; ++trial)
  {
    const int64_t left = static_cast<int64_t>(rng.nextU64InRange(0, 20)) - 10;
    const int64_t right = static_cast<int64_t>(rng.nextU64InRange(0, 20)) - 10;
    probes.push_back(pbt::BatchProbe{
        .source = std::format("    let pair{0} = ({1}, {2}); "
                              "let indexed{0} = pair{0}.0 + pair{0}.1; "
                              "let mut matched{0} = 0; "
                              "switch (pair{0}) {{ case (a, b) {{ matched{0} := a + b; }} }} "
                              "print(indexed{0} == matched{0});",
                              trial, left, right),
        .expect = "true",
        .label = std::format("({}, {})", left, right),
    });
  }
  const auto failure = pbt::runBatchProbes("tuple-switch", "import prelude;", probes);
  INFO("seed=" << seed);
  INFO("failure: " << failure.value_or(""));
  REQUIRE_FALSE(failure.has_value());
}

TEST_CASE("INC-33 tuple case negative contracts", "[INC33][TupleSwitch][Negative]")
{
  requireRejected("fun f(t: tuple<i64, i64>) -> i64 { switch (t) { case (a, b, c) { return a; } } }",
                  "tuple pattern binds 3 elements, but the scrutinee has 2");
  requireRejected("fun f(t: tuple<i64, i64>) -> i64 { switch (t) { case (a) { return a; } } }",
                  "expected `:` after binding name in type pattern");
  requireRejected("fun f(t: tuple<i64, i64>) -> i64 { switch (t) { case (a, b) { return a; } "
                  "case (c, d) { return d; } } }",
                  "duplicate tuple pattern in switch");
  requireRejected("fun f(t: tuple<i64, i64>) -> i64 { switch (t) { } }",
                  "switch is not exhaustive: missing a tuple pattern or `otherwise`");
  requireRejected("enum Color { Red, Blue } fun f(c: Color) -> i64 { switch (c) { "
                  "case (a, b) { return 0; } } }",
                  "tuple patterns require a tuple switch value");
  requireRejected("fun f(value: i64 | string) -> i64 { switch (value) { case (a, b) { return 0; } } }",
                  "union member pattern requires a `(name: Type)` binding");
}

TEST_CASE("INC-33 tuple_switch example runs end to end", "[INC33][TupleSwitch][Examples]")
{
  std::string output;
  std::string errors;
  std::ostringstream outputStream;
  std::ostringstream errorStream;
  std::string path{"example/tuple_switch.ng"};
  if (!std::filesystem::is_directory(std::filesystem::current_path() / "example"))
    path = "../example/tuple_switch.ng";
  const int status = NG::runDriver({path}, outputStream, errorStream);
  output = std::move(outputStream).str();
  errors = std::move(errorStream).str();
  REQUIRE(status == 0);
  REQUIRE(errors.empty());
  REQUIRE_THAT(output, ContainsSubstring("true\ntrue\ntrue"));
}
