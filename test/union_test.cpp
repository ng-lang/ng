// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "hir.hpp"
#include "pbt.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <sstream>
#include <string>
#include <vector>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  [[nodiscard]] auto resolve(std::string_view source) -> hir::Module
  {
    return hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
  }

  void check(std::string_view source)
  {
    static_cast<void>(typecheck::TypeChecker{}.check(resolve(source)));
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

  [[nodiscard]] auto runExample(std::string_view filename, std::string &output, std::string &errors) -> int
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({path}, outputStream, errorStream);
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
    REQUIRE(output.find(std::string{"native main exited with code "} + std::string{value} + "\n") != std::string::npos);
  }
} // namespace

TEST_CASE("vNext union types accept member values and compare against members", "[vNext][Union][Runtime]")
{
  expectValue("fun main() -> i64 { let x: i64 | string = 1; let y: i64 | string = \"hello\"; "
              "let mut total = 0; if (x == 1) { total := total + 1; } "
              "if (y == \"hello\") { total := total + 2; } return total; }",
              "3");
}

TEST_CASE("vNext union values flow through function parameters", "[vNext][Union][Runtime]")
{
  expectValue("fun read(value: bool | i64 | f64) -> i64 { if (value == 42) { return 7; } return 0; } "
              "fun main() -> i64 { let w: bool | i64 | f64 = 42; return read(w); }",
              "7");
}

TEST_CASE("vNext union validation rejects non-member values", "[vNext][Union][Errors]")
{
  try
  {
    check("fun main() -> i64 { let bad: bool | string = 5; return 0; }");
    FAIL("expected a non-member union error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "value of type i64 is not a member of union bool | string");
  }
}

TEST_CASE("vNext union switch matches member types, literals, and otherwise", "[vNext][Union][Runtime]")
{
  expectValue("fun classify(value: i64 | string) -> i64 { switch (value) { case 42 { return 100; } "
              "case (n: i64) { return n; } otherwise { return 7; } } } "
              "fun main() -> i64 { let a: i64 | string = 42; let b: i64 | string = 5; "
              "let c: i64 | string = \"hi\"; return classify(a) + classify(b) + classify(c); }",
              "112");
}

TEST_CASE("vNext union switch type patterns cover every member without otherwise", "[vNext][Union][Runtime]")
{
  expectValue("fun kind(value: i64 | string) -> i64 { switch (value) { case (n: i64) { return n; } "
              "case (s: string) { return 7; } } } "
              "fun main() -> i64 { let a: i64 | string = 5; let b: i64 | string = \"hi\"; "
              "return kind(a) + kind(b); }",
              "12");
}

TEST_CASE("vNext union switch matches string and float member literals", "[vNext][Union][Runtime]")
{
  // Regression: string member literals used to reach the integer payload
  // encoder, and a string compare against a non-string member dereferenced
  // the integer payload.
  expectValue("fun f(value: i64 | f64 | string) -> i64 { switch (value) { case \"go\" { return 1; } "
              "case 2.5 { return 3; } otherwise { return 2; } } } "
              "fun main() -> i64 { let a: i64 | f64 | string = \"go\"; let b: i64 | f64 | string = 2.5; "
              "let c: i64 | f64 | string = 7; return f(a) * 100 + f(b) * 10 + f(c); }",
              "132");
}

TEST_CASE("vNext union equality against a string skips non-string members", "[vNext][Union][Runtime]")
{
  expectValue("fun main() -> i64 { let c: i64 | string = 7; let s: i64 | string = \"go\"; let mut t = 0; "
              "if (c == \"go\") { t := t + 1; } if (s == \"go\") { t := t + 10; } "
              "if (c != \"go\") { t := t + 100; } return t; }",
              "110");
}

TEST_CASE("vNext union switch rejects a non-exhaustive member set", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (n: i64) { return n; } } }");
    FAIL("expected a non-exhaustive union switch error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} ==
            "switch is not exhaustive: missing member type `string` of union i64 | string "
            "(member-literal patterns match only their literal value, so add an `otherwise` or a type pattern "
            "for `string`)");
  }
}

TEST_CASE("vNext union switch rejects an unknown member type", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (b: bool) { return 1; } } }");
    FAIL("expected an unknown member type error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "`bool` is not a member type of union i64 | string");
  }
}

TEST_CASE("vNext union switch rejects a duplicate member pattern", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (a: i64) { return a; } "
          "case (b: i64) { return b; } } }");
    FAIL("expected a duplicate member pattern error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate member pattern for type `i64` in union switch");
  }
}

TEST_CASE("vNext union switch rejects a literal with no matching member family", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case true { return 1; } } }");
    FAIL("expected a literal member family error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "literal `true` does not match any member type of union i64 | string");
  }
}

TEST_CASE("vNext union switch rejects variant patterns", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case Foo { return 1; } } }");
    FAIL("expected a variant-pattern rejection");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "union switches match member types or literals, not variants (`Foo`)");
  }
}

TEST_CASE("vNext union switch requires a name: Type member pattern", "[vNext][Union][Errors]")
{
  try
  {
    check("fun f(value: i64 | string) -> i64 { switch (value) { case (i64) { return 1; } } }");
    FAIL("expected a type pattern binding parse error");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `:` after binding name in type pattern");
  }
}

TEST_CASE("vNext union example file runs end to end through ngi", "[vNext][Union][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/unions.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 123\n") != std::string::npos);
}

TEST_CASE("vNext union coercion boxes non-literal member values with their own tag", "[vNext][Union][Runtime]")
{
  // Regression: only literals were boxed on coercion; a local, call result,
  // or arithmetic value reached the union slot as a raw word (crash on the
  // first tag read), and `true` adopted the integer member of `bool | i64`.
  expectValue("struct H { v: i64 | string } "
              "fun k(value: i64 | string) -> i64 { switch (value) { case (n: i64) { return n; } "
              "case (s: string) { return 100; } } } "
              "fun mk(n: i64) -> i64 | string { return n; } "
              "fun main() -> i64 { let a = 3; let s = \"x\"; let mut u: i64 | string = 0; u := a; "
              "let mut t = k(u); u := s; t := t + k(u) + k(mk(a + 1)) + k(a + 2); "
              "let h = H { v: a }; t := t + k(h.v); let b = true; let x: bool | i64 = b; "
              "let y: bool | i64 = true; if (x == true) { t := t + 20; } if (y == true) { t := t + 40; } "
              "return t; }",
              "175");
}

namespace
{
  // ---- INC-23 PBT: union switch vs. a reference dispatcher -----------------

  enum class Member
  {
    Bool,
    I64,
    F64,
    String,
  };

  struct UnionValue
  {
    Member member{};
    bool boolean{};
    int64_t integer{};
    std::string text; // f64 literal text or string contents
  };

  struct UnionCase
  {
    enum class Kind
    {
      Type,
      Literal,
      Otherwise,
    } kind{};
    Member member{};                  // Type
    std::vector<UnionValue> literals; // Literal alternatives
  };

  [[nodiscard]] auto memberName(Member member) -> std::string_view
  {
    switch (member)
    {
    case Member::Bool:
      return "bool";
    case Member::I64:
      return "i64";
    case Member::F64:
      return "f64";
    case Member::String:
      return "string";
    }
    return "";
  }

  [[nodiscard]] auto sameValue(const UnionValue &left, const UnionValue &right) -> bool
  {
    if (left.member != right.member)
      return false;
    switch (left.member)
    {
    case Member::Bool:
      return left.boolean == right.boolean;
    case Member::I64:
      return left.integer == right.integer;
    case Member::F64:
      return std::stod(left.text) == std::stod(right.text);
    case Member::String:
      return left.text == right.text;
    }
    return false;
  }

  /// NG source for a value (a literal of its member family).
  [[nodiscard]] auto sourceOf(const UnionValue &value) -> std::string
  {
    switch (value.member)
    {
    case Member::Bool:
      return value.boolean ? "true" : "false";
    case Member::I64:
      return pbt::decimalLiteral(value.integer);
    case Member::F64:
      return value.text;
    case Member::String:
      return std::format("\"{}\"", value.text);
    }
    return "";
  }

  /// Small pools so generated values often hit generated literal patterns.
  /// Case-pattern floats stay non-negative (`case -2.5` does not parse).
  [[nodiscard]] auto drawValue(pbt::Rng &rng, Member member, bool forPattern) -> UnionValue
  {
    UnionValue value{.member = member};
    const bool wide = !forPattern && rng.below(0.25);
    switch (member)
    {
    case Member::Bool:
      value.boolean = rng.below(0.5);
      break;
    case Member::I64:
      value.integer = wide ? static_cast<int64_t>(rng.nextU64InRange(0, 2000)) - 1000
                           : static_cast<int64_t>(rng.nextU64InRange(0, 6)) - 3;
      break;
    case Member::F64:
    {
      static constexpr std::string_view pool[] = {"0.5", "1.5", "2.5", "10.25"};
      value.text = wide ? "-0.5" : std::string{pool[rng.nextU64InRange(0, 3)]};
      break;
    }
    case Member::String:
    {
      static constexpr std::string_view pool[] = {"a", "b", "cd", ""};
      value.text = wide ? std::format("w{}", rng.nextU64InRange(0, 999)) : std::string{pool[rng.nextU64InRange(0, 3)]};
      break;
    }
    }
    return value;
  }

  /// The reference dispatcher: first matching case in source order. A type
  /// pattern matches the active member; a literal matches the same member
  /// and value; `otherwise` matches everything. Returns the line the
  /// generated NG case body prints.
  [[nodiscard]] auto referenceDispatch(const std::vector<UnionCase> &cases, const UnionValue &value) -> std::string
  {
    for (size_t index = 0; index < cases.size(); ++index)
    {
      const auto &switchCase = cases[index];
      switch (switchCase.kind)
      {
      case UnionCase::Kind::Otherwise:
        return std::format("c{}", index);
      case UnionCase::Kind::Literal:
        if (std::ranges::any_of(switchCase.literals, [&](const auto &literal) { return sameValue(literal, value); }))
          return std::format("c{}", index);
        break;
      case UnionCase::Kind::Type:
        if (switchCase.member != value.member)
          break;
        switch (value.member)
        {
        case Member::Bool:
          return std::format("c{}:{}", index, value.boolean ? "true" : "false");
        case Member::I64:
          return std::format("c{}:{}", index, value.integer);
        case Member::F64:
          return std::format("c{}:f", index);
        case Member::String:
          return std::format("c{}:{}", index, value.text);
        }
      }
    }
    return "<no match>";
  }

  /// Case body source; type patterns echo their payload binding.
  [[nodiscard]] auto caseSource(const UnionCase &switchCase, size_t index) -> std::string
  {
    switch (switchCase.kind)
    {
    case UnionCase::Kind::Otherwise:
      return std::format("        otherwise {{ return \"c{}\"; }}\n", index);
    case UnionCase::Kind::Literal:
    {
      std::string alternatives;
      for (const auto &literal : switchCase.literals)
        alternatives += (alternatives.empty() ? "" : " | ") + sourceOf(literal);
      return std::format("        case {} {{ return \"c{}\"; }}\n", alternatives, index);
    }
    case UnionCase::Kind::Type:
      break;
    }
    const auto name = memberName(switchCase.member);
    switch (switchCase.member)
    {
    case Member::Bool:
    case Member::I64:
      return std::format("        case (p: {}) {{ return \"c{}:\" + toString(p); }}\n", name, index);
    case Member::F64:
      return std::format("        case (p: {}) {{ return \"c{}:f\"; }}\n", name, index);
    case Member::String:
      return std::format("        case (p: {}) {{ return \"c{}:\" + p; }}\n", name, index);
    }
    return "";
  }
} // namespace

TEST_CASE("INC-23 union switch dispatch matches the reference dispatcher (PBT)", "[vNext][Union][INC23][Pbt][Runtime]")
{
  const uint64_t seed = pbt::currentSeed("union-switch");
  pbt::Rng rng(seed);
  constexpr uint32_t kTrials = 24;
  constexpr size_t kValuesPerTrial = 6;

  std::string preamble = "import prelude;\n";
  std::vector<pbt::BatchProbe> probes;
  for (uint32_t trial = 0; trial < kTrials; ++trial)
  {
    // A union of 2..4 distinct builtin members in a random order.
    std::vector<Member> members{Member::Bool, Member::I64, Member::F64, Member::String};
    for (size_t index = members.size() - 1; index > 0; --index)
      std::swap(members[index], members[rng.nextU64InRange(0, index)]);
    members.resize(rng.nextU64InRange(2, 4));
    std::string unionType;
    for (const auto member : members)
      unionType += (unionType.empty() ? "" : " | ") + std::string{memberName(member)};

    // Type patterns for a random member subset plus 0..3 literal cases, in a
    // random order; `otherwise` is required unless every member has a type
    // pattern (static member-set exhaustiveness).
    std::vector<UnionCase> cases;
    size_t typed = 0;
    for (const auto member : members)
      if (rng.below(0.6))
      {
        cases.push_back(UnionCase{.kind = UnionCase::Kind::Type, .member = member});
        ++typed;
      }
    const auto literalCases = rng.nextU64InRange(0, 3);
    for (uint64_t index = 0; index < literalCases; ++index)
    {
      UnionCase literal{.kind = UnionCase::Kind::Literal};
      const auto alternatives = rng.nextU64InRange(1, 2);
      for (uint64_t alternative = 0; alternative < alternatives; ++alternative)
        literal.literals.push_back(drawValue(rng, members[rng.nextU64InRange(0, members.size() - 1)], true));
      cases.push_back(std::move(literal));
    }
    for (size_t index = cases.size(); index > 1; --index)
      std::swap(cases[index - 1], cases[rng.nextU64InRange(0, index - 1)]);
    if (typed < members.size() || rng.below(0.3))
      cases.push_back(UnionCase{.kind = UnionCase::Kind::Otherwise});

    std::string body;
    for (size_t index = 0; index < cases.size(); ++index)
      body += caseSource(cases[index], index);
    preamble += std::format("fun f{}(value: {}) -> string {{\n    switch (value) {{\n{}    }}\n}}\n", trial,
                            unionType, body);

    // Each value reaches the union through a literal binding, a local
    // binding, or a call argument (all three coercion sites box it).
    for (size_t index = 0; index < kValuesPerTrial; ++index)
    {
      const auto value = drawValue(rng, members[rng.nextU64InRange(0, members.size() - 1)], false);
      const auto literal = sourceOf(value);
      const auto route = rng.nextU64InRange(0, 2);
      std::string source;
      if (route == 0)
        source = std::format("    let v{0}_{1}: {2} = {3};\n    print(f{0}(v{0}_{1}));", trial, index, unionType, literal);
      else if (route == 1)
        source = std::format("    let r{0}_{1} = {3};\n    let v{0}_{1}: {2} = r{0}_{1};\n    print(f{0}(v{0}_{1}));",
                             trial, index, unionType, literal);
      else
        source = std::format("    let r{0}_{1} = {2};\n    print(f{0}(r{0}_{1}));", trial, index, literal);
      probes.push_back(pbt::BatchProbe{.source = std::move(source),
                                       .expect = referenceDispatch(cases, value),
                                       .label = std::format("f{} over {} with {} (route {})\n{}", trial, unionType,
                                                            literal, route, body)});
    }
  }

  const auto failure = pbt::runBatchProbes("inc23_union_switch", preamble, probes);
  if (failure.has_value())
    FAIL("replay with PBT_SEED=" << seed << ": " << *failure);
  REQUIRE(failure == std::nullopt);
}
