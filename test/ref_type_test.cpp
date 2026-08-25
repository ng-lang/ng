// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

#include <sstream>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  void check(std::string_view source)
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    static_cast<void>(typecheck::TypeChecker{}.check(module));
  }

  auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext type checker resolves scoped reference parameter types", "[vNext][Typecheck][Ref]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun sum(values: array<i64> ref) -> i64 { return 0; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto parameter = result.functionTypeIds.at(0).parameters.at(0);
  REQUIRE(result.typeDescriptors.at(parameter.value).kind == typecheck::TypeKind::Reference);
  REQUIRE_FALSE(result.typeDescriptors.at(parameter.value).referenceMutable);
  const auto element = result.typeDescriptors.at(parameter.value).element;
  REQUIRE(result.typeDescriptors.at(element.value).kind == typecheck::TypeKind::DynamicArray);
  REQUIRE(result.functionTypes.at(0).parameters.at(0) == "array<i64> ref");
}

TEST_CASE("vNext type checker distinguishes ref and ref mut", "[vNext][Typecheck][Ref]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun read(values: array<i64> ref) -> i64 { return 0; } "
      "fun write(values: array<i64> ref mut) -> i64 { return 0; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto read = result.functionTypeIds.at(0).parameters.at(0);
  const auto write = result.functionTypeIds.at(1).parameters.at(0);
  REQUIRE(read != write);
  REQUIRE(result.typeDescriptors.at(read.value).kind == typecheck::TypeKind::Reference);
  REQUIRE_FALSE(result.typeDescriptors.at(read.value).referenceMutable);
  REQUIRE(result.typeDescriptors.at(write.value).referenceMutable);
}

TEST_CASE("vNext type checker resolves raw pointer annotations as inert unsafe types", "[vNext][Typecheck][Ref]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun buffer(output: u8 *mut) { return; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto parameter = result.functionTypeIds.at(0).parameters.at(0);
  REQUIRE(result.typeDescriptors.at(parameter.value).kind == typecheck::TypeKind::RawPointer);
  REQUIRE(result.typeDescriptors.at(parameter.value).referenceMutable);
  REQUIRE(result.typeDescriptors.at(parameter.value).element == typecheck::builtin::U8);
}

TEST_CASE("vNext type checker types ref and ref mut expressions", "[vNext][Typecheck][Ref]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun main() -> i64 { let mut value = 1; if (true) { let read = ref value; } if (true) { let write = ref mut value; } "
      "return value; }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto read = result.typeIdOf(*module.functions.front().body.statements[1].consequence->statements.front().expression);
  const auto write = result.typeIdOf(*module.functions.front().body.statements[2].consequence->statements.front().expression);
  REQUIRE(result.typeDescriptors.at(read.value).kind == typecheck::TypeKind::Reference);
  REQUIRE_FALSE(result.typeDescriptors.at(read.value).referenceMutable);
  REQUIRE(result.typeDescriptors.at(read.value).element == typecheck::builtin::I64);
  REQUIRE(result.typeDescriptors.at(write.value).referenceMutable);
}

TEST_CASE("vNext type checker unifies references through generic parameters", "[vNext][Typecheck][Ref]")
{
  REQUIRE_NOTHROW(check("fun borrow<T>(value: T ref) -> T { return *value; } "
                        "fun main() -> i64 { let value = 1; let read = ref value; return borrow(read); }"));

  // `ref mut` must not unify with a `ref` parameter.
  try
  {
    check("fun borrow<T>(value: T ref) -> T { return *value; } "
          "fun main() -> i64 { let mut value = 1; let write = ref mut value; return borrow(write); }");
    FAIL("expected ref mutability mismatch");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "generic reference mutability mismatch");
  }
}

TEST_CASE("vNext type checker rejects dereferencing non-references", "[vNext][Typecheck][Ref]")
{
  try
  {
    check("fun main() { let value = 1; return *value; }");
    FAIL("expected deref of a non-reference");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot dereference value of type i64");
  }
}

TEST_CASE("vNext lowers and executes reference creation and dereference reads", "[vNext][Driver][Ref]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() -> i64 { let value = 1; let read = ref value; return *read; }"}, output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 1") != std::string::npos);
}
