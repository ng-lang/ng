// AI-generated code; reviewed for this repository's vNext rewrite.
// INC-14 slice S2c: higher-order functions. A function parameter with a
// closure type makes the declaration a template: calls that pass a concrete
// closure (literal or closure-typed local) are re-checked as closure-
// specialized instances in which calls through the parameter bind to the
// literal's synthesized code function. The original template body is only
// type-modeled (deferred closure calls) and lowered as a placeholder.
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
