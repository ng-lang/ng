// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "syntax/const_expr.hpp"
#include "syntax/module_parser.hpp"
#include "syntax/type_parser.hpp"

namespace syntax = NG::syntax;

namespace
{
  [[nodiscard]] auto asNamed(const syntax::TypeSyntaxPtr &type) -> const syntax::NamedTypeSyntax &
  {
    const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(type.get());
    REQUIRE(named != nullptr);
    return *named;
  }

  [[nodiscard]] auto asReference(const syntax::TypeSyntaxPtr &type) -> const syntax::ScopedReferenceTypeSyntax &
  {
    const auto *reference = dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(type.get());
    REQUIRE(reference != nullptr);
    return *reference;
  }

  [[nodiscard]] auto asRawPointer(const syntax::TypeSyntaxPtr &type) -> const syntax::RawPointerTypeSyntax &
  {
    const auto *pointer = dynamic_cast<const syntax::RawPointerTypeSyntax *>(type.get());
    REQUIRE(pointer != nullptr);
    return *pointer;
  }

  [[nodiscard]] auto asFunction(const syntax::ModuleItemPtr &item) -> const syntax::FunctionDeclaration &
  {
    const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get());
    REQUIRE(function != nullptr);
    return *function;
  }
} // namespace

TEST_CASE("vNext type parser preserves applied type and const arguments structurally", "[vNext][Syntax][Type]")
{
  const auto type = syntax::TypeParser{syntax::Lexer{}.lex("array<i64, 3>")}.parse();
  const auto *applied = dynamic_cast<const syntax::AppliedTypeSyntax *>(type.get());
  REQUIRE(applied != nullptr);
  REQUIRE(asNamed(applied->constructor).name == "array");
  REQUIRE(applied->arguments.size() == 2);
  REQUIRE(applied->arguments[0].kind == syntax::GenericArgumentKind::Type);
  REQUIRE(asNamed(applied->arguments[0].type).name == "i64");
  REQUIRE(applied->arguments[1].kind == syntax::GenericArgumentKind::ConstExpr);
  REQUIRE(applied->arguments[1].constExpr != nullptr);
  const auto *length = dynamic_cast<const syntax::ConstIntegerLiteral *>(applied->arguments[1].constExpr.get());
  REQUIRE(length != nullptr);
  REQUIRE(length->text == "3");
  REQUIRE(type->span.begin == 0);
  REQUIRE(type->span.end == 13);

  const auto nested = syntax::TypeParser{syntax::Lexer{}.lex("Result<array<i64>, Error>")}.parse();
  const auto *result = dynamic_cast<const syntax::AppliedTypeSyntax *>(nested.get());
  REQUIRE(result != nullptr);
  REQUIRE(result->arguments.size() == 2);
  REQUIRE(dynamic_cast<const syntax::AppliedTypeSyntax *>(result->arguments[0].type.get()) != nullptr);
}

TEST_CASE("vNext type parser supports canonical postfix ref and raw pointer syntax", "[vNext][Syntax][Type]")
{
  const auto readReference = syntax::TypeParser{syntax::Lexer{}.lex("Value ref")}.parse();
  const auto &reference = asReference(readReference);
  REQUIRE_FALSE(reference.isMutable);
  REQUIRE(asNamed(reference.target).name == "Value");
  REQUIRE(reference.span.begin == 0);
  REQUIRE(reference.span.end == 9);

  const auto mutableReference = syntax::TypeParser{syntax::Lexer{}.lex("Value ref mut")}.parse();
  REQUIRE(asReference(mutableReference).isMutable);

  const auto readPointer = syntax::TypeParser{syntax::Lexer{}.lex("u8 *const")}.parse();
  const auto &pointer = asRawPointer(readPointer);
  REQUIRE_FALSE(pointer.isMutable);
  REQUIRE(asNamed(pointer.pointee).name == "u8");

  const auto mutablePointer = syntax::TypeParser{syntax::Lexer{}.lex("u8 *mut")}.parse();
  REQUIRE(asRawPointer(mutablePointer).isMutable);
}

TEST_CASE("vNext module parser preserves typed parameters and returns in syntax AST", "[vNext][Syntax][Type]")
{
  const auto unit = syntax::parseSourceUnit("fun choose(a: Value ref, output: u8 *mut) -> Result ref { output }");
  REQUIRE(unit.items.size() == 1);
  const auto &function = asFunction(unit.items[0]);
  REQUIRE(function.parameters.size() == 2);
  REQUIRE(function.parameters[0].name == "a");
  REQUIRE_FALSE(asReference(function.parameters[0].type).isMutable);
  REQUIRE(function.parameters[1].name == "output");
  REQUIRE(asRawPointer(function.parameters[1].type).isMutable);
  REQUIRE(function.returnType != nullptr);
  REQUIRE_FALSE(asReference(function.returnType).isMutable);
}

TEST_CASE("vNext type parser gives precise malformed raw pointer diagnostics", "[vNext][Syntax][Type]")
{
  try
  {
    static_cast<void>(syntax::TypeParser{syntax::Lexer{}.lex("u8 *")}.parse());
    FAIL("expected raw pointer type to require const or mut");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `const` or `mut` after `*` in raw pointer type");
    REQUIRE(error.span().begin == 4);
    REQUIRE(error.span().end == 4);
  }
}

TEST_CASE("vNext module parser diagnoses omitted parameter type annotations", "[vNext][Syntax][Type]")
{
  try
  {
    static_cast<void>(syntax::parseSourceUnit("fun value(input) { input }"));
    FAIL("expected parameter annotation to be required");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected `:` after parameter name");
    REQUIRE(error.span().begin == 15);
    REQUIRE(error.span().end == 16);
  }
}
