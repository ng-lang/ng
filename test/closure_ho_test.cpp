// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-14 slice S2c: higher-order functions. A function parameter with a
// closure type makes the declaration a template: calls that pass a concrete
// closure (literal or closure-typed local) are re-checked as closure-
// specialized instances in which calls through the parameter bind to the
// literal's synthesized code function. The original template body is only
// type-modeled (deferred closure calls) and lowered as a placeholder.
// INC-14 slice S4: closure-param forwarding — a closure-typed parameter
// passed into another higher-order call records a closure target (with its
// capture group, remapped to the forwarding instance's own capture params),
// so nested higher-order chains specialize and thread captures correctly.
// INC-14 slice S4 (generic): generic closure literals in generic function
// bodies. A closure whose signature names type parameters is model-typed
// only (no code function exists until the enclosing generic is instantiated
// with concrete types); calls through model closures defer via the
// placeholder path, concrete instances synthesize real lowered code, and
// closure-typed bindings survive function cloning intact.
#include "test.hpp"

#include "driver.hpp"

#include <sstream>

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

  [[nodiscard]] auto expectValue(std::string_view source, std::string_view value) -> void
  {
    std::string output;
    std::string errors;
    REQUIRE(run(source, output, errors) == 0);
    INFO("errors: " << errors);
    REQUIRE(errors.empty());
    REQUIRE(output.find(std::string{"native main exited with code "} + std::string{value} + "\n") !=
            std::string::npos);
  }

  [[nodiscard]] auto expectError(std::string_view source, std::string_view message) -> void
  {
    std::string output;
    std::string errors;
    REQUIRE(run(source, output, errors) != 0);
    INFO("output: " << output);
    REQUIRE(errors.find(message) != std::string::npos);
  }
} // namespace

TEST_CASE("vNext non-generic higher-order function calls a closure parameter", "[vNext][Closure][S2c]")
{
  expectValue("fun apply(f: |i64| -> i64, x: i64) -> i64 { return f(x); }"
              "fun main() -> i64 { return apply(|v| v * 2, 21); }",
              "42");
}

TEST_CASE("vNext generic higher-order function monomorphizes a closure argument", "[vNext][Closure][S2c]")
{
  expectValue("fun applyTwice<T>(f: |T| -> T, x: T) -> T { return f(f(x)); }"
              "fun main() -> i64 { return applyTwice(|v| v * 2, 21); }",
              "84");
}

TEST_CASE("vNext higher-order call passes a closure-typed local", "[vNext][Closure][S2c]")
{
  expectValue("fun apply(f: |i64| -> i64, x: i64) -> i64 { return f(x); }"
              "fun main() -> i64 { let f: |i64| -> i64 = |v| v + 1; return apply(f, 41); }",
              "42");
}

TEST_CASE("vNext higher-order closure may use a block body", "[vNext][Closure][S2c]")
{
  expectValue("fun apply(f: |i64| -> i64, x: i64) -> i64 { return f(x); }"
              "fun main() -> i64 { return apply(|v| { let y = v * 3; y + 1 }, 10); }",
              "31");
}

TEST_CASE("vNext higher-order multi-parameter closure argument", "[vNext][Closure][S2c]")
{
  expectValue("fun combine(f: |i64, i64| -> i64, a: i64, b: i64) -> i64 { return f(a, b); }"
              "fun main() -> i64 { return combine(|u, v| u * v, 6, 7); }",
              "42");
}

TEST_CASE("vNext higher-order with a nested call through the closure", "[vNext][Closure][S2c]")
{
  expectValue("fun applies<T>(f: |T| -> T, x: T, n: T) -> T {"
              "  let mut acc = n; acc := f(acc); acc := f(acc); return acc; }"
              "fun main() -> i64 { return applies(|v| v + 1, 0, 40); }",
              "42");
}

TEST_CASE("vNext unused higher-order template type-checks as a placeholder", "[vNext][Closure][S2c]")
{
  expectValue("fun stale(f: |i64| -> i64) -> i64 { return f(1); }"
              "fun main() -> i64 { return 0; }",
              "0");
}

TEST_CASE("vNext higher-order template closure arity is checked", "[vNext][Closure][S2c]")
{
  expectError("fun bad(f: |i64| -> i64, x: i64) -> i64 { return f(); }"
              "fun main() -> i64 { return bad(|v| v, 1); }",
              "closure argument count mismatch: expected 1, got 0");
}

TEST_CASE("vNext closure alias without a literal remains rejected (S1 contract)", "[vNext][Closure][S2c]")
{
  expectError("fun main() -> i64 { let f = |x: i64| x + 1; let g = f; return g(41); }",
              "closure value `g` was not created directly by a closure literal");
}

TEST_CASE("vNext std.seq map with a closure literal runs end to end", "[vNext][Closure][S2c]")
{
  expectValue("import prelude; import seq;"
              "fun main() -> i64 { let xs = [1, 2, 3, 4]; let ys = map(xs, |x| x * 2);"
              "  return ys[0] + ys[1] + ys[2] + ys[3]; }",
              "20");
}

TEST_CASE("vNext higher-order call threads a copy-captured scalar", "[vNext][Closure][S3]")
{
  expectValue("fun apply(f: |i64, i64| -> i64, a: i64, b: i64) -> i64 { return f(a, b); }"
              "fun main() -> i64 { let base = 100; let f = [base] |x: i64, y: i64| x + y + base;"
              "  return apply(f, 5, 7); }",
              "112");
}

TEST_CASE("vNext higher-order call threads a copy-captured array", "[vNext][Closure][S3]")
{
  expectValue("fun call(f: |i64| -> i64, x: i64) -> i64 { return f(x); }"
              "fun main() -> i64 { let a = [1, 2, 3]; let f = [a] |x: i64| x + a[0];"
              "  return call(f, 1); }",
              "2");
}

TEST_CASE("vNext higher-order call threads a move-captured array", "[vNext][Closure][S3]")
{
  expectValue("fun call(f: |i64| -> i64, x: i64) -> i64 { return f(x); }"
              "fun main() -> i64 { let a = [4, 5, 6]; let f = [move a] |x: i64| x + a[1];"
              "  return call(f, 2); }",
              "7");
}

TEST_CASE("vNext higher-order call threads multiple captures", "[vNext][Closure][S3]")
{
  expectValue("fun apply(f: |i64, i64| -> i64, a: i64, b: i64) -> i64 { return f(a, b); }"
              "fun main() -> i64 { let lo = 10; let hi = 20;"
              "  let f = [lo, hi] |x: i64, y: i64| lo + hi * x; return apply(f, 2, 0); }",
              "50");
}

TEST_CASE("vNext higher-order capture groups are per-call-site independent", "[vNext][Closure][S3]")
{
  expectValue("fun apply(f: |i64, i64| -> i64, a: i64, b: i64) -> i64 { return f(a, b); }"
              "fun main() -> i64 { let base = 100; let f = [base] |x: i64, y: i64| x + y + base;"
              "  return apply(f, 5, 7) + apply(f, 1, 2); }",
              "215");
}

TEST_CASE("vNext higher-order uncaptured enclosing local is rejected (S3 contract)", "[vNext][Closure][S3]")
{
  expectError("fun apply(f: |i64| -> i64, x: i64) -> i64 { return f(x); }"
              "fun main() -> i64 { let secret = 9; let f = |v: i64| v + secret;"
              "  return apply(f, 1); }",
              "which is not listed in the capture list");
}

TEST_CASE("vNext move capture consumes the affine original local (S3)", "[vNext][Closure][S3]")
{
  expectError("struct S { v: i64 }"
              "fun mk() -> S { return S { v: 4 }; }"
              "fun main() -> i64 { let s = mk(); let h = [move s] |x: i64| x + s.v; let z = s; return 0; }",
              "use of moved value `s`");
}

TEST_CASE("vNext move capture consumes the original array local (S3)", "[vNext][Closure][S3]")
{
  expectError("fun main() -> i64 { let a = [1, 2, 3]; let h = [move a] |x: i64| x + a[0];"
              "  let z = a; return 0; }",
              "use of moved value `a`");
}

TEST_CASE("vNext copy capture leaves the original usable (S3)", "[vNext][Closure][S3]")
{
  expectValue("fun main() -> i64 { let a = [1, 2, 3]; let h = [a] |x: i64| x + a[0];"
              "  return h(1) + a[0]; }",
              "3");
}

TEST_CASE("vNext higher-order closure parameter forwards into another higher-order call (S4)", "[vNext][Closure][S4]")
{
  expectValue("fun mapClosure<T>(values: array<T>, f: |T| -> T) -> array<T> { return map(values, f); }"
              "import prelude; import seq;"
              "fun main() -> i64 { let ys = mapClosure([1, 2, 3], |x: i64| x * 2);"
              "  return ys[0] + ys[1] + ys[2]; }",
              "12");
}

TEST_CASE("vNext forwarded closure parameter threads its capture group (S4)", "[vNext][Closure][S4]")
{
  expectValue("fun mapClosure<T>(values: array<T>, f: |T| -> T) -> array<T> { return map(values, f); }"
              "import prelude; import seq;"
              "fun main() -> i64 { let factor: i64 = 5;"
              "  let ys = mapClosure([1, 2, 3], [factor] |x: i64| x * factor);"
              "  return ys[0] + ys[1] + ys[2]; }",
              "30");
}

TEST_CASE("vNext closure parameter forwards through a multi-level chain (S4)", "[vNext][Closure][S4]")
{
  expectValue("fun apply2<T>(x: T, f: |T| -> T) -> T { return f(x); }"
              "fun apply3<T>(x: T, f: |T| -> T) -> T { return apply2(x, f); }"
              "fun apply4<T>(x: T, f: |T| -> T) -> T { return apply3(x, f); }"
              "fun main() -> i64 { let offs: i64 = 3; return apply4(4, [offs] |x: i64| x + offs); }",
              "7");
}

TEST_CASE("vNext forwarded multi-parameter closure with capture (S4)", "[vNext][Closure][S4]")
{
  expectValue("fun biMap<T>(x: T, y: T, f: |T, T| -> T) -> T { return f(x, y); }"
              "fun biMapF<T>(x: T, y: T, f: |T, T| -> T) -> T { return biMap(x, y, f); }"
              "fun main() -> i64 { let offs: i64 = 10;"
              "  return biMapF(3, 4, [offs] |a: i64, b: i64| a * b + offs); }",
              "22");
}

TEST_CASE("vNext literal capture threads into a direct higher-order call (S4 regression)", "[vNext][Closure][S4]")
{
  expectValue("import prelude; import seq;"
              "fun main() -> i64 { let factor: i64 = 3;"
              "  let ys = map([1, 2], [factor] |x: i64| x * factor);"
              "  return ys[0] + ys[1]; }",
              "9");
}

TEST_CASE("vNext generic closure local calls through a type parameter (S4)", "[vNext][Closure][S4]")
{
  expectValue("import prelude;"
              "fun genUse<T>(x: T, y: T) -> T { let f: |T| -> T = |v: T| v; return f(x); }"
              "fun main() -> i64 { return genUse(21, 0); }",
              "21");
}

TEST_CASE("vNext generic closure local forwards into a higher-order call (S4)", "[vNext][Closure][S4]")
{
  expectValue("import prelude; import seq;"
              "fun genMap<T>(xs: array<T>) -> array<T> { let id: |T| -> T = |v: T| v; return map(xs, id); }"
              "fun main() -> i64 { let ys = genMap([5, 6, 7]); return ys[0] + ys[1] + ys[2]; }",
              "18");
}

TEST_CASE("vNext generic closure captures a type-parameter local (S4)", "[vNext][Closure][S4]")
{
  expectValue("import prelude;"
              "fun genAdd<T>(base: T, x: T) -> T { let g = [base] |v: T| base; return g(x); }"
              "fun main() -> i64 { return genAdd(40, 2); }",
              "40");
}

TEST_CASE("vNext generic body with a concrete closure literal instantiates (S4)", "[vNext][Closure][S4]")
{
  expectValue("import prelude;"
              "fun makeT<T>(x: T, k: i64) -> T { let f: |i64| -> i64 = |v: i64| v;"
              "  let r: i64 = f(1); return x; }"
              "fun main() -> i64 { return makeT(41, 1); }",
              "41");
}

TEST_CASE("vNext op-using generic closure body is rejected at model time (S4)", "[vNext][Closure][S4+]")
{
  std::string output;
  std::string errors;
  const int status = run("import prelude;"
                         "fun genBad<T>(x: T) -> T { let f = |v: T| v * v; return f(x); }"
                         "fun main() -> i64 { return genBad(2); }",
                         output, errors);
  REQUIRE(status != 0);
  REQUIRE(errors.find("binary operand type mismatch") != std::string::npos);
}

TEST_CASE("vNext std.seq filter keeps elements the predicate accepts (S5)", "[vNext][Closure][S5]")
{
  expectValue("import prelude; import seq;"
              "fun main() -> i64 { let evens = filter([1, 2, 3, 4, 5, 6], |x| x % 2 == 0);"
              "  return evens[0] + evens[1] + evens[2]; }",
              "12");
}

TEST_CASE("vNext std.seq fold threads an accumulator through a two-parameter closure (S5)", "[vNext][Closure][S5]")
{
  expectValue("import prelude; import seq;"
              "fun main() -> i64 { return fold([1, 2, 3, 4, 5], 0, |acc, x| acc + x); }",
              "15");
}

TEST_CASE("vNext std.seq fold threads a captured accumulator offset (S5)", "[vNext][Closure][S5]")
{
  expectValue("import prelude; import seq;"
              "fun main() -> i64 { let offs: i64 = 10;"
              "  return fold([1, 2, 3], 0, [offs] |acc, x| acc + x + offs); }",
              "36");
}

TEST_CASE("vNext std.seq filter threads a captured lower bound (S5)", "[vNext][Closure][S5]")
{
  expectValue("import prelude; import seq;"
              "fun main() -> i64 { let lo: i64 = 2;"
              "  let kept = filter([1, 2, 3, 4], [lo] |x| x >= lo);"
              "  return kept[0] + kept[1] + kept[2]; }",
              "9");
}

TEST_CASE("vNext generic wrapper forwards its closure into std.seq fold (S5)", "[vNext][Closure][S5]")
{
  expectValue("import prelude; import seq;"
              "fun myFold<T, U>(values: array<T>, initial: U, f: |U, T| -> U) -> U {"
              "  return fold(values, initial, f); }"
              "fun main() -> i64 { return myFold([1, 2, 3], 0, |acc, x| acc + x); }",
              "6");
}

TEST_CASE("vNext function returning a closure is rejected as escaping (S5 negative)", "[vNext][Closure][S5]")
{
  expectError("fun makeAdd(double: bool) -> |i64| -> i64 {"
              "  if (double) { return |x| x * 2; }"
              "  return |x| x; }",
              "which cannot escape its scope");
}

TEST_CASE("vNext closure type satisfies the Fn/FnOnce auto-trait bounds (S5)", "[vNext][Closure][S5]")
{
  expectValue("import prelude;"
              "fun runFn<T>(f: T) -> i64 where T: Fn { return 1; }"
              "fun runOnce<T>(f: T) -> i64 where T: FnOnce { return 2; }"
              "fun main() -> i64 { let double: |i64| -> i64 = |x: i64| x * 2;"
              "  return runFn(double) + runOnce(double); }",
              "3");
}

TEST_CASE("vNext non-closure type fails the Fn auto-trait bound (S5 negative)", "[vNext][Closure][S5]")
{
  expectError("import prelude;"
              "fun runFn<T>(f: T) -> i64 where T: Fn { return 1; }"
              "fun main() -> i64 { return runFn(3); }",
              "does not satisfy its where clause");
}
