// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/driver.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"

#include <filesystem>
#include <sstream>

namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;

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
    const int status = NG::vnext::runDriver({"--source", source}, outputStream, errorStream);
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
    const int status = NG::vnext::runDriver({path}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  constexpr std::string_view CounterShow =
      "struct Counter { value: i64 } "
      "trait Show { fun show(self: Self ref) -> i64; } "
      "impl Show for Counter { fun show(self: Self ref) -> i64 { return (*self).value; } } ";
} // namespace

TEST_CASE("vNext module parser builds trait and impl declarations", "[vNext][Trait][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(
      "trait Eq { fun same(self: Self ref, other: Self ref) -> bool; } "
      "trait Ord: Eq { fun less(self: Self ref, other: Self ref) -> bool { return false; } } "
      "struct Person { name: string } "
      "impl Ord for Person { fun same(self: Self ref, other: Self ref) -> bool { return true; } "
      "fun less(self: Self ref, other: Self ref) -> bool { return false; } }");
  REQUIRE(unit.items.size() == 4);
  const auto &eq = *static_cast<const syntax::TraitDeclaration *>(unit.items[0].get());
  REQUIRE(eq.name == "Eq");
  REQUIRE(eq.supertraits.empty());
  REQUIRE(eq.methods.size() == 1);
  const auto &ord = *static_cast<const syntax::TraitDeclaration *>(unit.items[1].get());
  REQUIRE(ord.supertraits == std::vector<std::string>{"Eq"});
  REQUIRE(ord.methods[0].body.has_value());
  const auto &impl = *static_cast<const syntax::ImplDeclaration *>(unit.items[3].get());
  REQUIRE(impl.traitName == "Ord");
  REQUIRE(impl.methods.size() == 2);
}

TEST_CASE("vNext resolver lowers trait methods into module functions", "[vNext][Trait][Hir]")
{
  const auto module = resolve(std::string{CounterShow} + "fun main() -> i64 { return 0; }");
  REQUIRE(module.traits.size() == 1);
  REQUIRE(module.impls.size() == 1);
  REQUIRE(module.impls[0].methodIds.size() == 1);
  // main plus the lowered impl method.
  REQUIRE(module.functions.size() == 2);
  REQUIRE(module.functions[1].name.starts_with("impl$Show$show$Counter"));
}

TEST_CASE("vNext static dispatch calls impl methods on concrete receivers", "[vNext][Trait][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run(std::string{CounterShow} +
                  "fun main() -> i64 { let counter = Counter { value: 7 }; return counter.show(); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 7") != std::string::npos);
}

TEST_CASE("vNext mutating methods borrow receivers mutably", "[vNext][Trait][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("struct Counter { value: i64 } "
              "trait Increment { fun inc(self: Self ref mut) -> unit; } "
              "impl Increment for Counter { fun inc(self: Self ref mut) -> unit { (*self).value := (*self).value + 1; } } "
              "fun main() -> i64 { let mut counter = Counter { value: 1 }; counter.inc(); counter.inc(); return counter.value; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 3") != std::string::npos);

  try
  {
    check("struct Counter { value: i64 } "
          "trait Increment { fun inc(self: Self ref mut) -> unit; } "
          "impl Increment for Counter { fun inc(self: Self ref mut) -> unit { return; } } "
          "fun main() { let counter = Counter { value: 1 }; counter.inc(); }");
    FAIL("expected an immutable receiver error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot create a mutable reference to an immutable binding");
  }
}

TEST_CASE("vNext impls satisfy supertrait methods and default methods", "[vNext][Trait][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("struct Counter { value: i64 } "
              "trait Eq { fun same(self: Self ref, other: Self ref) -> bool; } "
              "trait Ord: Eq { fun less(self: Self ref, other: Self ref) -> bool; } "
              "impl Ord for Counter { fun same(self: Self ref, other: Self ref) -> bool { return (*self).value == (*other).value; } "
              "fun less(self: Self ref, other: Self ref) -> bool { return (*self).value < (*other).value; } } "
              "fun main() -> i64 { let one = Counter { value: 1 }; let two = Counter { value: 2 }; let mut total = 0; "
              "if (one.less(ref two)) { total := total + 1; } if (one.same(ref one)) { total := total + 2; } return total; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 3") != std::string::npos);

  REQUIRE(run("struct Person { name: string } "
              "trait Display { fun text(self: Self ref) -> string { return \"unknown\"; } } "
              "impl Display for Person { } "
              "fun main() -> i64 { let ada = Person { name: \"Ada\" }; let text = Display.text(ada); "
              "if (text == \"unknown\") { return 1; } return 0; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 1") != std::string::npos);
}

TEST_CASE("vNext trait bounds in where clauses require impl evidence", "[vNext][Trait][Typecheck]")
{
  std::string output;
  std::string errors;
  REQUIRE(run(std::string{CounterShow} +
                  "fun describe<T>(value: T ref) -> i64 where T: Show { return 32; } "
                  "fun main() -> i64 { let counter = Counter { value: 1 }; let read = ref counter; return describe(read); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 32") != std::string::npos);

  try
  {
    check(std::string{CounterShow} +
          "fun describe<T>(value: T ref) -> i64 where T: Show { return 32; } "
          "fun main() -> i64 { let plain = 7; let read = ref plain; return describe(read); }");
    FAIL("expected an unsatisfied trait bound error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "call to `describe` does not satisfy its where clause");
  }
}

TEST_CASE("vNext trait checking rejects incoherent and incomplete impls", "[vNext][Trait][Errors]")
{
  try
  {
    check(std::string{CounterShow} + "impl Show for Counter { fun show(self: Self ref) -> i64 { return 0; } }");
    FAIL("expected a duplicate impl error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate impl for trait `Show`");
  }

  try
  {
    check("struct Counter { value: i64 } "
          "trait Show { fun show(self: Self ref) -> i64; fun extra(self: Self ref) -> i64; } "
          "impl Show for Counter { fun show(self: Self ref) -> i64 { return 0; } }");
    FAIL("expected a missing method error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "impl for trait `Show` is missing method `extra`");
  }

  try
  {
    check("struct Counter { value: i64 } "
          "trait Show { fun show(self: Self ref) -> i64; } "
          "impl Show for Counter { fun show(self: Self ref) -> i64 { return 0; } fun extra(self: Self ref) -> i64 { return 1; } }");
    FAIL("expected an unknown method error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "impl for trait `Show` provides unknown method `extra`");
  }

  try
  {
    check("struct Counter { value: i64 } fun f<T: Missing>(x: T) -> i64 { return 0; }");
    FAIL("expected an unknown trait bound error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "unknown trait bound `Missing` on `T`");
  }

  try
  {
    check("struct Counter { value: i64 } "
          "trait Show { fun show(self: Self ref) -> i64; } "
          "fun render<T>(value: T ref) -> i64 { return value.show(); } "
          "fun main() -> i64 { return 0; }");
    FAIL("expected a missing bound error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "no trait bound provides method `show` for type parameter `T`");
  }
}

TEST_CASE("vNext method calls through bounded type parameters monomorphize per instance", "[vNext][Trait][Monomorphize]")
{
  std::string output;
  std::string errors;
  REQUIRE(run("struct Counter { value: i64 } "
              "trait Show { fun show(self: Self ref) -> i64; } "
              "impl Show for Counter { fun show(self: Self ref) -> i64 { return (*self).value; } } "
              "fun render<T: Show>(value: T ref) -> i64 { return value.show(); } "
              "fun main() -> i64 { let counter = Counter { value: 7 }; let read = ref counter; return render(read); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 7") != std::string::npos);

  REQUIRE(run("struct Box { value: i64 } "
              "trait Show { fun show(self: Self ref) -> i64; } "
              "impl Show for Box { fun show(self: Self ref) -> i64 { return (*self).value + 1; } } "
              "fun render<T: Show>(value: T ref) -> i64 { return value.show(); } "
              "fun main() -> i64 { let box = Box { value: 6 }; let read = ref box; return render(read); }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 7") != std::string::npos);
}

TEST_CASE("vNext traits example file runs end to end through ngi", "[vNext][Trait][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/traits.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 66") != std::string::npos);
}
