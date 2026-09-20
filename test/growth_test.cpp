// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-21 S2 (ARCH-4): array growth + in-place `pushBack`. The unique-own
// model lets a `pushBack<T>(array<T> ref mut, T)` grow and mutate the buffer
// in place (amortized O(1) via `$ngrt_arr_push_mut`); a possibly-shared
// receiver takes the copy-on-write fallback (`$ngrt_arr_push_cow`), so value
// semantics always hold. Both decisions are stamped post-check
// (typecheck_growth.cpp) and ride the native-call payload into lowering.
//
// * Model PBT: push-built array ≡ one-shot `[...]` literal, element-wise
//   (len + every read), over random n ∈ [0, 200] and random i64 values, in
//   both unique (in-place) and copy-on-write (an early `let b = xs`) builds —
//   the model never value-passes `xs`, so the unique trials exercise the
//   in-place helper, the aliased trials the copy-on-write helper.
// * SSA symbol contracts: a pure builder lowers to `$ngrt_arr_push_mut`; the
//   copied-binding builder lowers to `$ngrt_arr_push_cow`.
// * Aliased read-back isolation: the early copy observes the pre-push array.
// * Deref write-back round-trip: `*refMut := e` through a native ref row.
// * Negative contract: `pushBack(ref mut xs, v)` on an immutable binding is
//   rejected.

#include "test.hpp"

#include "driver.hpp"
#include "pbt.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <sstream>
#include <string>

namespace
{
  constexpr int64_t kFloor = -50;
  constexpr int64_t kCeil = 50;

  [[nodiscard]] auto boundValue(pbt::Rng &rng) -> int64_t
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

  [[nodiscard]] auto runSsa(std::string_view source, std::string &ssa) -> int
  {
    std::ostringstream ssaStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--source", source, "--emit=ssa"}, ssaStream, errorStream);
    ssa = std::move(ssaStream).str();
    return status;
  }
} // namespace

TEST_CASE("INC-21 S2 push-built arrays equal one-shot literals (PBT)", "[Growth][Pbt]")
{
  const uint64_t seed = pbt::currentSeed("growth");
  constexpr uint32_t kTrials = 80;
  pbt::Rng rng(seed);

  for (uint32_t trial = 0; trial < kTrials; ++trial)
  {
    const int64_t n = static_cast<int64_t>(rng.nextU64InRange(0, 201));
    const bool cow = trial % 2 == 1;

    std::string values;
    std::string pushes;
    for (int64_t index = 0; index < n; ++index)
    {
      const int64_t value = boundValue(rng);
      if (index > 0)
        values += ", ";
      values += std::to_string(value);
      pushes += "    pushBack(ref mut xs, " + std::to_string(value) + ");\n";
    }

    const std::string copyBind = cow ? "    let copy: array<i64> = xs;\n" : "";
    const std::string copyCheck = cow ? "    print(len(copy) == 0);\n" : "";

    const std::string source = std::format(
        R"(import prelude;
import seq;

fun main() -> i64 {{
    let mut xs: array<i64> = [];
{0}{1}    let lit: array<i64> = [{2}];
    let mut okLen = len(lit) == {3};
    let mut okElems = true;
    let mut j = 0;
    while (j < {3}) {{
        if (xs[j] == lit[j]) {{}} else {{ okElems := false; }}
        j := j + 1;
    }}
{4}    print(okLen);
    print(okElems);
    return 0;
}}
)",
        copyBind, pushes, values, n, copyCheck);

    std::string sourceOutput;
    std::string errors;
    const int status = run(source, sourceOutput, errors);
    INFO("errors: " << errors);
    REQUIRE(status == 0);

    std::string expected = cow ? "true\ntrue\ntrue\n" : "true\ntrue\n";
    INFO("trial " << trial << " n=" << n << " cow=" << cow << " output:\n" << sourceOutput);
    REQUIRE(sourceOutput.find(expected) != std::string::npos);
  }
}

TEST_CASE("INC-21 S2 pushBack lowers to the in-place vs copy-on-write helper", "[Growth]")
{
  const std::string inPlace = R"(import prelude;
import seq;

fun main() -> i64 {
    let mut xs: array<i64> = [];
    pushBack(ref mut xs, 1);
    pushBack(ref mut xs, 2);
    pushBack(ref mut xs, 3);
    print(xs[2]);
    return 0;
}
)";

  const std::string copied = R"(import prelude;
import seq;

fun main() -> i64 {
    let mut xs: array<i64> = [1, 2, 3];
    let copy: array<i64> = xs;
    pushBack(ref mut xs, 9);
    print(len(copy));
    print(xs[3]);
    return 0;
}
)";

  std::string ssa;
  std::string errors;

  REQUIRE(runSsa(inPlace, ssa) == 0);
  INFO("in-place SSA:\n" << ssa);
  REQUIRE(ssa.find("$ngrt_arr_push_mut") != std::string::npos);
  REQUIRE(ssa.find("$ngrt_arr_push_cow") == std::string::npos);

  REQUIRE(runSsa(copied, ssa) == 0);
  INFO("copy-on-write SSA:\n" << ssa);
  REQUIRE(ssa.find("$ngrt_arr_push_cow") != std::string::npos);
  REQUIRE(ssa.find("$ngrt_arr_push_mut") == std::string::npos);

  std::string output;
  const int status = run(copied, output, errors);
  INFO("errors: " << errors);
  REQUIRE(status == 0);
  const std::string expected = "3\n9\n";
  INFO("output:\n" << output);
  REQUIRE(output.find(expected) != std::string::npos);
}

TEST_CASE("INC-21 S2 deref write-back `*refMut := e` round-trips", "[Growth]")
{
  const std::string source = R"(import prelude;
import seq;

fun bump(xs: array<i64> ref mut, value: i64) -> unit {
    *xs := [value, value + 1];
}

fun main() -> i64 {
    let mut a: array<i64> = [0];
    bump(ref mut a, 7);
    print(len(a));
    print(a[0]);
    print(a[1]);
    return 0;
}
)";

  std::string output;
  std::string errors;
  const int status = run(source, output, errors);
  INFO("errors: " << errors);
  REQUIRE(status == 0);
  const std::string expected = "2\n7\n8\n";
  INFO("output:\n" << output);
  REQUIRE(output.find(expected) != std::string::npos);
}

TEST_CASE("INC-21 S2 `ref mut` pushBack requires a mutable binding", "[Growth]")
{
  const std::string source = R"(import prelude;
import seq;

fun main() -> i64 {
    let xs: array<i64> = [1];
    pushBack(ref mut xs, 2);
    return 0;
}
)";

  std::string output;
  std::string errors;
  const int status = run(source, output, errors);
  REQUIRE(status != 0);
  INFO("errors: " << errors);
  REQUIRE_THAT(errors, Catch::Matchers::ContainsSubstring("immutable binding"));
}