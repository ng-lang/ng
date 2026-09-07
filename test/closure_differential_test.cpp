// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-14 S6: differential PBT. The closure combinators (std.seq map/fold)
// must agree with the language's builtin spread forms over generated arrays
// x a stable of pure functions:
//   map(xs, |v| f(v))  ==  [f(xs)...]          (map comprehension)
//   fold(xs, seed, |a,v| g(a,v))  ==  g(seed, xs...)   (fold spread)
// Element-wise equality via a same() helper makes map compare both length and
// contents; fold compares the accumulated value. Values stay small (|x|,
// |seed| <= 15, len <= 6) so checked arithmetic cannot trap. Uses the INC-1
// harness (deterministic xoshiro256**, replay with PBT_SEED).
#include "test.hpp"
#include "pbt.hpp"

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
  // Shared NG module head: imports plus the pure function stable and the
  // element-wise array equality helper.
  constexpr std::string_view kPreamble = R"NG(
import prelude;
import seq;

fun same(a: array<i64>, b: array<i64>) -> bool {
    let mut i = 0;
    if (len(a) != len(b)) {
        return false;
    }
    while (i < len(a)) {
        if (a[i] != b[i]) {
            return false;
        }
        i := i + 1;
    }
    return true;
}

fun f0(x: i64) -> i64 { return x + 5; }
fun f1(x: i64) -> i64 { return x * 2 - 3; }
fun f2(x: i64) -> i64 { return x * x + 1; }
fun f3(x: i64) -> i64 { return (x + 4) * 3; }
fun f4(x: i64) -> i64 { return 10 - x; }

fun g0(acc: i64, x: i64) -> i64 { return acc + x; }
fun g1(acc: i64, x: i64) -> i64 { return acc * x + 1; }
fun g2(acc: i64, x: i64) -> i64 { return acc - x; }
fun g3(acc: i64, x: i64) -> i64 { return acc + x * 3; }
)NG";

  constexpr uint32_t kTrials = 300;

  [[nodiscard]] auto drawValue(pbt::Rng &rng) -> int64_t
  {
    return static_cast<int64_t>(rng.nextU64InRange(0, 30)) - 15;
  }

  [[nodiscard]] auto arrayLiteral(pbt::Rng &rng) -> std::string
  {
    const size_t length = rng.nextU64InRange(0, 6);
    std::string text = "[";
    for (size_t index = 0; index < length; ++index)
    {
      if (index > 0)
        text += ", ";
      text += pbt::decimalLiteral(drawValue(rng));
    }
    text += "]";
    return text;
  }

  [[nodiscard]] auto mapProbe(pbt::Rng &rng, uint32_t trial) -> pbt::BatchProbe
  {
    const size_t fn = rng.nextU64InRange(0, 4);
    const auto xs = arrayLiteral(rng);
    // The comprehension is bound to a local first: a comprehension array
    // literal nested directly inside another call argument cannot be pushed
    // through overload candidate scoring (its element type needs expected-type
    // context, and the scorer swallows the inner TypeError).
    return {std::format("let xs{}: array<i64> = {};\n"
                        "    let z{} = [f{}(xs{})...];\n"
                        "    print(same(map(xs{}, |v: i64| f{}(v)), z{}));",
                        trial, xs, trial, fn, trial, trial, fn, trial),
            "true", std::format("map({}) equals comprehension f{}", xs, fn)};
  }

  [[nodiscard]] auto foldProbe(pbt::Rng &rng, uint32_t trial) -> pbt::BatchProbe
  {
    const size_t fn = rng.nextU64InRange(0, 3);
    const auto xs = arrayLiteral(rng);
    const auto seed = pbt::decimalLiteral(drawValue(rng));
    return {std::format("let ys{}: array<i64> = {}; print(fold(ys{}, {}, |acc: i64, v: i64| g{}(acc, v)) == g{}({}, ys{}...));",
                        trial, xs, trial, seed, fn, fn, seed, trial),
            "true", std::format("fold({}, {}) equals spread g{}", xs, seed, fn)};
  }
} // namespace

TEST_CASE("INC-14 S6 differential: closure map/fold equal the builtin spread forms", "[INC14][S6][Differential]")
{
  const uint64_t seed = pbt::currentSeed("s6-closure-diff");
  pbt::Rng rng{seed};
  std::vector<pbt::BatchProbe> probes;
  probes.reserve(2 * kTrials);
  for (uint32_t trial = 0; trial < kTrials; ++trial)
  {
    probes.push_back(mapProbe(rng, trial));
    probes.push_back(foldProbe(rng, trial));
  }
  const auto failure = pbt::runBatchProbes("s6_closure_diff", kPreamble, probes);
  if (failure.has_value())
    FAIL("replay with PBT_SEED=" << seed << ": " << *failure);
  REQUIRE(failure == std::nullopt);
}