// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

#include <filesystem>
#include <format>
#include <random>
#include <sstream>

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

TEST_CASE("vNext derived clone deep-copies independently of the source", "[vNext][Derive][Runtime]")
{
  expectValue("import prelude; struct Point: derive(Copy + Clone) { x: i64, y: i64, } "
              "fun main() -> i64 { "
              "let point = Point { x: 20, y: 22 }; "
              "let mut cloned = point.clone(); "
              "cloned.x := 30; "
              "let mut total = 0; "
              "if (point.x == 20 && cloned.x == 30) { total := total + 1; } "
              "if (point.y == 22 && cloned.y == 22) { total := total + 2; } "
              "return total; }",
              "3");
}

TEST_CASE("vNext derived Copy and Clone satisfy trait bounds and where clauses", "[vNext][Derive][Runtime]")
{
  expectValue("import prelude; struct Point: derive(Copy + Clone) { x: i64, } "
              "fun require_copy<T: Copy>() -> i64 { return 1; } "
              "fun require_clone<T: Clone>() -> i64 { return 2; } "
              "fun require_copy_where<T>(value: T) -> i64 where T: Copy { return 4; } "
              "fun main() -> i64 { let point = Point { x: 1 }; "
              "return require_copy<Point>() + require_clone<Point>() + require_copy_where(point); }",
              "7");
}

TEST_CASE("vNext auto traits apply to every concrete type", "[vNext][Derive][Runtime]")
{
  expectValue("auto trait Send {} "
              "fun require_send<T: Send>() -> i64 { return 5; } "
              "fun require_send_where<T>(value: T) -> i64 where T: Send { return 6; } "
              "fun main() -> i64 { return require_send<i64>() + require_send<string>() + require_send_where(true); }",
              "16");
}

TEST_CASE("vNext derive validation reports malformed derives", "[vNext][Derive][Errors]")
{
  {
    std::string output;
    std::string errors;
    REQUIRE(run("import prelude; struct Point: derive(Unknown) { x: i64, } fun main() -> i64 { return 0; }",
                output, errors) != 0);
    REQUIRE(errors.find("derive: unknown trait `Unknown`; import a module that declares it") != std::string::npos);
  }

  {
    std::string output;
    std::string errors;
    REQUIRE(run("import prelude; struct Point: derive(Copy + Copy) { x: i64, } fun main() -> i64 { return 0; }",
                output, errors) != 0);
    REQUIRE(errors.find("duplicate derive for trait `Copy` on type `Point`") != std::string::npos);
  }

  {
    std::string output;
    std::string errors;
    REQUIRE(run("import prelude; struct Point: derive(Copy) { x: i64, } impl Copy for Point {} "
                "fun main() -> i64 { return 0; }",
                output, errors) != 0);
    REQUIRE(errors.find("derive conflicts with explicit impl for trait `Copy` on type `Point`") != std::string::npos);
  }

  {
    std::string output;
    std::string errors;
    REQUIRE(run("import prelude; struct Point: derive(Copy) { x: i64, } "
                "fun main() -> i64 { let point = Point { x: 1 }; let bad = point.clone(); return 0; }",
                output, errors) != 0);
    REQUIRE(errors.find("no method `clone` for value of type Point") != std::string::npos);
  }
}

TEST_CASE("vNext derive example file runs end to end through ngi", "[vNext][Derive][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/derive.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 255\n") != std::string::npos);
}

TEST_CASE("vNext derive(Eq) synthesizes field-level struct equality", "[vNext][Derive][Eq][Runtime]")
{
  expectValue("import prelude; struct Point: derive(Eq) { x: i64, y: i64, } "
              "fun main() -> i64 { "
              "let p1 = Point { x: 1, y: 2 }; "
              "let p2 = Point { x: 1, y: 2 }; "
              "let p3 = Point { x: 1, y: 3 }; "
              "let mut total = 0; "
              "if (p1 == p2) { total := total + 1; } "
              "if (p1 != p3) { total := total + 2; } "
              "if (!(p1 == p3)) { total := total + 4; } "
              "return total; }",
              "7");
}

TEST_CASE("vNext derive(Eq) works for nested structs", "[vNext][Derive][Eq][Runtime]")
{
  expectValue("import prelude; struct Inner: derive(Eq) { a: i64, b: i64, } "
              "struct Outer: derive(Eq) { x: i64, inner: Inner, } "
              "fun main() -> i64 { "
              "let o1 = Outer { x: 1, inner: Inner { a: 10, b: 20 } }; "
              "let o2 = Outer { x: 1, inner: Inner { a: 10, b: 20 } }; "
              "let o3 = Outer { x: 1, inner: Inner { a: 10, b: 99 } }; "
              "let mut total = 0; "
              "if (o1 == o2) { total := total + 1; } "
              "if (o1 != o3) { total := total + 2; } "
              "return total; }",
              "3");
}

TEST_CASE("vNext derive(Eq) error: non-Eq field type", "[vNext][Derive][Eq][Errors]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; struct Inner { a: i64, } "
              "struct Outer: derive(Eq) { inner: Inner, } "
              "fun main() -> i64 { return 0; }",
              output, errors) != 0);
  REQUIRE(errors.find("derive(Eq) on `Outer`: field `inner` type `Inner` does not derive Eq") != std::string::npos);
}

TEST_CASE("vNext derive(Eq) error: float field not Eq-comparable", "[vNext][Derive][Eq][Errors]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; struct Bad: derive(Eq) { x: f64, } fun main() -> i64 { return 0; }",
              output, errors) != 0);
  REQUIRE(errors.find("derive(Eq) on `Bad`: field `x` type `f64` is not Eq-comparable") != std::string::npos);
}

TEST_CASE("vNext derive(Show) synthesizes name{field: value, ...} golden strings", "[vNext][Derive][Show][Runtime]")
{
  std::string output;
  std::string errors;
  const std::string source =
      "import prelude; "
      "struct Inner: derive(Show) { a: i64, b: i64, } "
      "struct Point: derive(Show) { x: i64, y: i64, } "
      "struct Mixed: derive(Show) { n: i32, flag: bool, ratio: f64, part: Inner, label: string, } "
      "struct Tagged: derive(Show) { name: string, } "
      "struct Empty: derive(Show) { } "
      "fun main() -> i64 { "
      "let p = Point { x: 20, y: 22 }; print(p.show()); "
      "let m = Mixed { n: 7, flag: true, ratio: 2.5, part: Inner { a: 1, b: 2 }, label: \"hi\" }; print(m.show()); "
      "let t = Tagged { name: \"cube\" }; print(t.show()); "
      "let e = Empty { }; print(e.show()); "
      "return 0; }";
  REQUIRE(run(source, output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("Point{x: 20, y: 22}\n") != std::string::npos);
  REQUIRE(output.find("Mixed{n: 7, flag: true, ratio: 2.5, part: Inner{a: 1, b: 2}, label: hi}\n") != std::string::npos);
  REQUIRE(output.find("Tagged{name: cube}\n") != std::string::npos);
  REQUIRE(output.find("Empty{}\n") != std::string::npos);
}

TEST_CASE("vNext derive(Show) property: output contains the type name and every field label",
          "[vNext][Derive][Show][Property]")
{
  // Generate random simple structs; the runtime show() output must contain
  // the type name and every field label (plan INC-11 property slice), and
  // equal the format built here from the same source data.
  std::minstd_rand random{11};
  const char *const kinds[] = {"i64", "string", "bool"};
  constexpr size_t sampleCount = 24;
  std::string source = "import prelude; ";
  std::vector<std::string> expectedLines;
  std::vector<size_t> fieldCounts;
  for (size_t n = 0; n < sampleCount; ++n)
  {
    const std::string name = std::format("Gen_{}", n);
    const size_t fieldCount = 1 + random() % 4;
    fieldCounts.push_back(fieldCount);
    std::vector<std::pair<std::string, std::string>> fields; // label -> display value
    std::string structText = std::format("struct {}: derive(Show) {{ ", name);
    std::string runText = std::format(" fun run_{}() -> string {{ let v = {}{{ ", name, name);
    for (size_t f = 0; f < fieldCount; ++f)
    {
      const std::string label = std::format("f_{}", f);
      const std::string kind = kinds[random() % 3];
      const size_t choice = random() % 3;
      std::string ngValue;
      std::string display;
      if (kind == "i64")
      {
        ngValue = std::to_string(choice + 1);
        display = ngValue;
      }
      else if (kind == "string")
      {
        ngValue = std::format("\"v{}\"", choice);
        display = std::format("v{}", choice);
      }
      else
      {
        ngValue = choice % 2 == 0 ? "true" : "false";
        display = ngValue;
      }
      structText += std::format("{}: {}, ", label, kind);
      runText += std::format("{}: {}, ", label, ngValue);
      fields.emplace_back(label, display);
    }
    structText += "} ";
    runText += "}; return v.show(); } ";
    source += structText + runText;
    std::string expected = name + "{";
    for (size_t i = 0; i < fields.size(); ++i)
      expected += fields[i].first + ": " + fields[i].second + (i + 1 < fields.size() ? ", " : "");
    expected += "}";
    expectedLines.push_back(std::move(expected));
  }
  source += "fun main() -> i64 { ";
  for (size_t n = 0; n < sampleCount; ++n)
    source += std::format("print(run_{}()); ", std::format("Gen_{}", n));
  source += "return 0; }";

  std::string output;
  std::string errors;
  REQUIRE(run(source, output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  for (size_t n = 0; n < sampleCount; ++n)
  {
    INFO("sample " << n);
    const std::string name = std::format("Gen_{}", n);
    const std::string &expected = expectedLines[n];
    const auto begin = output.find(name + "{");
    REQUIRE(begin != std::string::npos);
    const auto end = output.find('\n', begin);
    REQUIRE(end != std::string::npos);
    const std::string line = output.substr(begin, end - begin);
    REQUIRE(line == expected);
    // Plan property slice: the output contains the type name and each label.
    REQUIRE(line.find(name) != std::string::npos);
    for (size_t f = 0; f < fieldCounts[n]; ++f)
      REQUIRE(line.find(std::format("f_{}", f)) != std::string::npos);
  }
}

TEST_CASE("vNext derive(Show) errors: unshowable fields and misuse", "[vNext][Derive][Show][Errors]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("import prelude; struct Bad: derive(Show) { x: i8, } fun main() -> i64 { return 0; }", output, errors) != 0);
  REQUIRE(errors.find("derive(Show) on `Bad`: field `x` type `i8` is not showable (no toString overload)") !=
          std::string::npos);

  REQUIRE(run("import prelude; struct Inner { a: i64, } "
              "struct Outer: derive(Show) { inner: Inner, } fun main() -> i64 { return 0; }", output, errors) != 0);
  REQUIRE(errors.find("derive(Show) on `Outer`: field `inner` type `Inner` does not derive Show") != std::string::npos);

  REQUIRE(run("import prelude; struct Plain { x: i64, } "
              "fun main() -> i64 { let p = Plain { x: 1 }; let s = p.show(); return 0; }", output, errors) != 0);
  REQUIRE(errors.find("no method `show` for value of type Plain") != std::string::npos);

  REQUIRE(run("import prelude; struct Point: derive(Show) { x: i64, } "
              "fun main() -> i64 { let p = Point { x: 1 }; let s = p.show(2); return 0; }", output, errors) != 0);
  REQUIRE(errors.find("derived show takes no arguments") != std::string::npos);
}
