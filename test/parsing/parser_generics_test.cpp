#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

// ============================================================================
// Generic Function Definitions
// ============================================================================

TEST_CASE("parser should parse generic function with single type param", "[Parser][Generics]")
{
  auto ast = parse("fun<T> identity(x: T) -> T { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module;
  REQUIRE(mod != nullptr);
  REQUIRE(mod->definitions.size() == 1);

  auto funDef = dynamic_ast_cast<FunctionDef>(mod->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->funName == "identity");
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "T");
  REQUIRE(funDef->genericParams[0]->isPack == false);
  REQUIRE(funDef->genericParams[0]->bound == nullptr);

  // Check function params use the generic type
  REQUIRE(funDef->params.size() == 1);
  REQUIRE(funDef->params[0]->paramName == "x");
  REQUIRE(funDef->params[0]->annotatedType != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "T");

  // Check return type
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse const generic parameters and arguments", "[Parser][Generics][Const]")
{
  auto ast = parse(R"(
        type Buffer<T, const N: u32> = native;
        fun<const N: u32> make_repeat(value: i32) -> array<i32, N> = native;
        val xs: array<i32, 4u8> = [1, 2, 3, 4];
    )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module->definitions.size() == 3);

  auto typeAlias = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(typeAlias != nullptr);
  REQUIRE(typeAlias->genericParams.size() == 2);
  REQUIRE(typeAlias->genericParams[1]->isConst);
  REQUIRE(typeAlias->genericParams[1]->name == "N");
  REQUIRE(typeAlias->genericParams[1]->constType != nullptr);
  REQUIRE(typeAlias->genericParams[1]->constType->name == "u32");

  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[2]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation->name == "array");
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 2);
  REQUIRE(valStmt->typeAnnotation->genericArgs[1]->constLiteral);
  REQUIRE(valStmt->typeAnnotation->genericArgs[1]->name == "4u8");
  REQUIRE(valStmt->typeAnnotation->genericArgs[1]->constLiteralType == "u8");

  destroyast(ast);
}

// ============================================================================
// Suffix Generic Syntax: `T TypeName` => TypeName<T>
// ============================================================================

TEST_CASE("parser should parse suffix generic syntax: bool vector", "[Parser][Generics][Suffix]")
{
  auto ast = parse("val x: bool vector = [];");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module;
  REQUIRE(mod != nullptr);
  REQUIRE(mod->definitions.size() == 1);

  auto valDef = dynamic_ast_cast<ValDef>(mod->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);
  REQUIRE(valStmt->typeAnnotation->name == "vector");
  REQUIRE(valStmt->typeAnnotation->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 1);
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->name == "bool");
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->type == TypeAnnotationType::BUILTIN_BOOL);

  destroyast(ast);
}

TEST_CASE("parser should parse suffix generic with numeric type: i32 vector", "[Parser][Generics][Suffix]")
{
  auto ast = parse("val xs: i32 vector = [];");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);
  REQUIRE(valStmt->typeAnnotation->name == "vector");
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 1);
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->name == "i32");
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->type == TypeAnnotationType::BUILTIN_I32);

  destroyast(ast);
}

TEST_CASE("parser should preserve suffix generic names: i32 array", "[Parser][Generics][Suffix]")
{
  auto ast = parse("val xs: i32 array = [];");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);
  REQUIRE(valStmt->typeAnnotation->name == "array");
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 1);
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->name == "i32");
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->type == TypeAnnotationType::BUILTIN_I32);

  destroyast(ast);
}

TEST_CASE("parser should parse suffix generic with left-associative nesting: i32 Optional List",
          "[Parser][Generics][Suffix]")
{
  // `i32 Optional List` desugars to `List<Optional<i32>>`
  auto ast = parse("val x: i32 Optional List = unit;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);

  // Outer: List<...>
  REQUIRE(valStmt->typeAnnotation->name == "List");
  REQUIRE(valStmt->typeAnnotation->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 1);

  // Inner: Optional<i32>
  auto inner = valStmt->typeAnnotation->genericArgs[0];
  REQUIRE(inner->name == "Optional");
  REQUIRE(inner->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(inner->genericArgs.size() == 1);
  REQUIRE(inner->genericArgs[0]->name == "i32");
  REQUIRE(inner->genericArgs[0]->type == TypeAnnotationType::BUILTIN_I32);

  destroyast(ast);
}

TEST_CASE("parser should parse multi-param suffix generic: (string, i32) Map", "[Parser][Generics][Suffix]")
{
  // `(string, i32) Map` desugars to `Map<string, i32>`
  auto ast = parse("val m: (string, i32) Map = unit;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);

  // Should be Map<string, i32>
  REQUIRE(valStmt->typeAnnotation->name == "Map");
  REQUIRE(valStmt->typeAnnotation->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 2);
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->name == "string");
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->type == TypeAnnotationType::BUILTIN_STRING);
  REQUIRE(valStmt->typeAnnotation->genericArgs[1]->name == "i32");
  REQUIRE(valStmt->typeAnnotation->genericArgs[1]->type == TypeAnnotationType::BUILTIN_I32);

  destroyast(ast);
}

TEST_CASE("parser should parse multi-param suffix with left-associative nesting: (string, i32) Map Optional",
          "[Parser][Generics][Suffix]")
{
  // `(string, i32) Map Optional` desugars to `Optional<Map<string, i32>>`
  auto ast = parse("val m: (string, i32) Map Optional = unit;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);

  // Outer: Optional<...>
  REQUIRE(valStmt->typeAnnotation->name == "Optional");
  REQUIRE(valStmt->typeAnnotation->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 1);

  // Inner: Map<string, i32>
  auto inner = valStmt->typeAnnotation->genericArgs[0];
  REQUIRE(inner->name == "Map");
  REQUIRE(inner->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(inner->genericArgs.size() == 2);
  REQUIRE(inner->genericArgs[0]->name == "string");
  REQUIRE(inner->genericArgs[1]->name == "i32");

  destroyast(ast);
}

TEST_CASE("parser should parse suffix generic in function signature", "[Parser][Generics][Suffix]")
{
  // Suffix generic in parameter and return type positions
  auto ast = parse("fun process(xs: i32 vector) -> bool vector { return []; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->funName == "process");

  // Param: `i32 vector` => vector<i32>
  REQUIRE(funDef->params.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "vector");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "i32");

  // Return: `bool vector` => vector<bool>
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->name == "vector");
  REQUIRE(funDef->returnType->genericArgs.size() == 1);
  REQUIRE(funDef->returnType->genericArgs[0]->name == "bool");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with multiple type params", "[Parser][Generics]")
{
  auto ast = parse("fun<T, U> pair(a: T, b: U) -> T { return a; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 2);
  REQUIRE(funDef->genericParams[0]->name == "T");
  REQUIRE(funDef->genericParams[0]->isPack == false);
  REQUIRE(funDef->genericParams[1]->name == "U");
  REQUIRE(funDef->genericParams[1]->isPack == false);

  // Check both params use different generic types
  REQUIRE(funDef->params.size() == 2);
  REQUIRE(funDef->params[0]->annotatedType->name == "T");
  REQUIRE(funDef->params[1]->annotatedType->name == "U");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with three type params", "[Parser][Generics]")
{
  auto ast = parse("fun<A, B, C> triple(a: A, b: B, c: C) -> A { return a; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 3);
  REQUIRE(funDef->genericParams[0]->name == "A");
  REQUIRE(funDef->genericParams[1]->name == "B");
  REQUIRE(funDef->genericParams[2]->name == "C");

  REQUIRE(funDef->params.size() == 3);
  REQUIRE(funDef->params[0]->annotatedType->name == "A");
  REQUIRE(funDef->params[1]->annotatedType->name == "B");
  REQUIRE(funDef->params[2]->annotatedType->name == "C");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with pack parameter", "[Parser][Generics][Pack]")
{
  auto ast = parse("fun<T...> make_tuple(args: T) -> T { return args; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "T");
  REQUIRE(funDef->genericParams[0]->isPack == true);

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with type bound", "[Parser][Generics]")
{
  auto ast = parse("fun<T: Comparable> do_sort(items: T) -> T { return items; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "T");
  REQUIRE(funDef->genericParams[0]->bound != nullptr);
  REQUIRE(funDef->genericParams[0]->bound->name == "Comparable");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with name-before-angle-bracket syntax", "[Parser][Generics]")
{
  // Also support: fun name<T>(...) after the function name
  auto ast = parse("fun identity<T>(x: T) -> T { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->funName == "identity");
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with name-before-angle-bracket and multiple params",
          "[Parser][Generics]")
{
  auto ast = parse("fun convert<A, B>(v: A) -> B { return v; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->funName == "convert");
  REQUIRE(funDef->genericParams.size() == 2);
  REQUIRE(funDef->genericParams[0]->name == "A");
  REQUIRE(funDef->genericParams[1]->name == "B");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with expression body", "[Parser][Generics]")
{
  auto ast = parse("fun<T> identity(x: T) -> T = x;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->funName == "identity");
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "T");
  REQUIRE(funDef->body != nullptr);

  destroyast(ast);
}

TEST_CASE("parser should parse generic function without params (no generics)", "[Parser][Generics]")
{
  auto ast = parse("fun simple(x: int) -> int { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 0);

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with no generic-typed parameters", "[Parser][Generics]")
{
  // Function has generic params but uses them only in return type
  auto ast = parse("fun<T> mk_default() -> T { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "T");
  REQUIRE(funDef->params.size() == 0);
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse exported generic function", "[Parser][Generics]")
{
  auto ast = parse("export fun<T> identity(x: T) -> T { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module;
  REQUIRE(mod != nullptr);
  REQUIRE(mod->exports.size() == 1);
  REQUIRE(mod->exports[0] == "identity");

  auto funDef = dynamic_ast_cast<FunctionDef>(mod->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with mixed bound and pack params", "[Parser][Generics][Pack]")
{
  auto ast = parse("fun<T: Hashable, U...> process(key: T, vals: U) -> T { return key; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 2);
  REQUIRE(funDef->genericParams[0]->name == "T");
  REQUIRE(funDef->genericParams[0]->isPack == false);
  REQUIRE(funDef->genericParams[0]->bound != nullptr);
  REQUIRE(funDef->genericParams[0]->bound->name == "Hashable");
  REQUIRE(funDef->genericParams[1]->name == "U");
  REQUIRE(funDef->genericParams[1]->isPack == true);
  REQUIRE(funDef->genericParams[1]->bound == nullptr);

  destroyast(ast);
}

// ============================================================================
// Generic Type Definitions (TypeDef)
// ============================================================================

TEST_CASE("parser should parse generic TypeDef with single param", "[Parser][Generics][TypeDef]")
{
  auto ast = parse("type Wrapper<T> { property value: T; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto typeDef = dynamic_ast_cast<TypeDef>(compileUnit->module->definitions[0]);
  REQUIRE(typeDef != nullptr);
  REQUIRE(typeDef->typeName == "Wrapper");
  REQUIRE(typeDef->genericParams.size() == 1);
  REQUIRE(typeDef->genericParams[0]->name == "T");
  REQUIRE(typeDef->properties.size() == 1);

  destroyast(ast);
}

TEST_CASE("parser should parse generic TypeDef with multiple params", "[Parser][Generics][TypeDef]")
{
  auto ast = parse("type Pair<A, B> { property first: A; property second: B; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto typeDef = dynamic_ast_cast<TypeDef>(compileUnit->module->definitions[0]);
  REQUIRE(typeDef != nullptr);
  REQUIRE(typeDef->typeName == "Pair");
  REQUIRE(typeDef->genericParams.size() == 2);
  REQUIRE(typeDef->genericParams[0]->name == "A");
  REQUIRE(typeDef->genericParams[1]->name == "B");
  REQUIRE(typeDef->properties.size() == 2);

  destroyast(ast);
}

TEST_CASE("parser should parse generic TypeDef with member functions", "[Parser][Generics][TypeDef]")
{
  auto ast = parse("type Container<T> { property value: T; fun get(self: ref<Self>) -> T { return self.value; } }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto typeDef = dynamic_ast_cast<TypeDef>(compileUnit->module->definitions[0]);
  REQUIRE(typeDef != nullptr);
  REQUIRE(typeDef->typeName == "Container");
  REQUIRE(typeDef->genericParams.size() == 1);
  REQUIRE(typeDef->genericParams[0]->name == "T");
  REQUIRE(typeDef->properties.size() == 1);
  REQUIRE(typeDef->memberFunctions.size() == 1);

  destroyast(ast);
}

TEST_CASE("parser should parse non-generic TypeDef (no regression)", "[Parser][Generics][TypeDef]")
{
  auto ast = parse("type Point { property x: i32; property y: i32; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto typeDef = dynamic_ast_cast<TypeDef>(compileUnit->module->definitions[0]);
  REQUIRE(typeDef != nullptr);
  REQUIRE(typeDef->typeName == "Point");
  REQUIRE(typeDef->genericParams.size() == 0);

  destroyast(ast);
}

// ============================================================================
// Type Alias with Generics
// ============================================================================

TEST_CASE("parser should parse type alias with generic params", "[Parser][Generics][TypeAlias]")
{
  auto ast = parse("type Pair<A, B> = (A, B);");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto aliasDef = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(aliasDef != nullptr);
  REQUIRE(aliasDef->aliasName == "Pair");
  REQUIRE(aliasDef->genericParams.size() == 2);
  REQUIRE(aliasDef->genericParams[0]->name == "A");
  REQUIRE(aliasDef->genericParams[1]->name == "B");

  destroyast(ast);
}

TEST_CASE("parser should parse type alias with single generic param", "[Parser][Generics][TypeAlias]")
{
  auto ast = parse("type Ref<T> = T;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto aliasDef = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(aliasDef != nullptr);
  REQUIRE(aliasDef->aliasName == "Ref");
  REQUIRE(aliasDef->genericParams.size() == 1);
  REQUIRE(aliasDef->genericParams[0]->name == "T");
  REQUIRE(aliasDef->underlyingType != nullptr);
  REQUIRE(aliasDef->underlyingType->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse generic type partial specialization", "[Parser][Generics][TypeAlias]")
{
  auto ast = parse("type<T> deref<ref<T>> = T;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto aliasDef = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(aliasDef != nullptr);
  REQUIRE(aliasDef->aliasName == "deref");
  REQUIRE(aliasDef->genericParams.size() == 1);
  REQUIRE(aliasDef->specializationPattern != nullptr);
  REQUIRE(aliasDef->specializationPattern->name == "deref");
  REQUIRE(aliasDef->underlyingType != nullptr);
  REQUIRE(aliasDef->underlyingType->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse abstract generic type alias declarations", "[Parser][Generics][TypeAlias]")
{
  auto ast = parse("type deref<T>;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto aliasDef = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(aliasDef != nullptr);
  REQUIRE(aliasDef->aliasName == "deref");
  REQUIRE(aliasDef->genericParams.size() == 1);
  REQUIRE(aliasDef->abstract);
  REQUIRE(aliasDef->underlyingType == nullptr);
  REQUIRE(aliasDef->repr() == "type deref<T>;");

  destroyast(ast);
}

TEST_CASE("parser should parse deleted ref specialization", "[Parser][Generics][TypeAlias]")
{
  auto ast = parse("type<T> ref<ref<T>> = delete;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto aliasDef = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(aliasDef != nullptr);
  REQUIRE(aliasDef->aliasName == "ref");
  REQUIRE(aliasDef->genericParams.size() == 1);
  REQUIRE(aliasDef->specializationPattern != nullptr);
  REQUIRE(aliasDef->deleted);

  destroyast(ast);
}

TEST_CASE("parser should parse deleted generic function declarations", "[Parser][Generics][Delete]")
{
  auto ast = parse("fun<T> accept(value: ref<T>) = delete;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->funName == "accept");
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->deleted);
  REQUIRE(funDef->body == nullptr);

  destroyast(ast);
}

TEST_CASE("parser should parse deleted const specializations", "[Parser][Generics][Delete]")
{
  auto ast = parse("const<T> is_bad<ref<T>>: bool = delete;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto constDef = dynamic_ast_cast<ConstDef>(compileUnit->module->definitions[0]);
  REQUIRE(constDef != nullptr);
  REQUIRE(constDef->constName == "is_bad");
  REQUIRE(constDef->genericParams.size() == 1);
  REQUIRE(constDef->specializationPattern != nullptr);
  REQUIRE(constDef->deleted);

  destroyast(ast);
}

TEST_CASE("parser should parse type specialization where predicates", "[Parser][Generics][TypeAlias]")
{
  auto ast = parse("type<T> deref<ref<T>>: where is_ref<T> = T;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto aliasDef = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(aliasDef != nullptr);
  REQUIRE(aliasDef->aliasName == "deref");
  REQUIRE(aliasDef->specializationPattern != nullptr);
  REQUIRE(aliasDef->whereBounds.size() == 1);
  REQUIRE(aliasDef->whereBounds[0]->predicate != nullptr);
  REQUIRE(aliasDef->whereBounds[0]->predicate->repr() == "is_ref<T>");

  destroyast(ast);
}

TEST_CASE("parser should parse type specialization trait constraints", "[Parser][Generics][TypeAlias]")
{
  auto ast = parse("type<T> wrapper<T>: where T: Show + Debug && is_ref<T> = T;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto aliasDef = dynamic_ast_cast<TypeAliasDef>(compileUnit->module->definitions[0]);
  REQUIRE(aliasDef != nullptr);
  REQUIRE(aliasDef->whereBounds.size() == 3);
  REQUIRE(aliasDef->whereBounds[0]->repr() == "T: Show");
  REQUIRE(aliasDef->whereBounds[1]->repr() == "T: Debug");
  REQUIRE(aliasDef->whereBounds[2]->repr() == "is_ref<T>");
  REQUIRE(aliasDef->repr() == "type <T> wrapper<T>: where T: Show && T: Debug && is_ref<T> = T;");

  destroyast(ast);
}

TEST_CASE("parser should reject type alias trait list constraints", "[Parser][Generics][TypeAlias][Error]")
{
  parseInvalid("type<T> wrapper<T>: Show + Debug where is_ref<T> = T;",
               "Type alias constraint section only supports `: where ...`");
}

TEST_CASE("parser should parse native const predicate declarations", "[Parser][Generics][ConstPredicate]")
{
  auto ast = parse("const is_ref<T>: bool = native;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto constDef = dynamic_ast_cast<ConstDef>(compileUnit->module->definitions[0]);
  REQUIRE(constDef != nullptr);
  REQUIRE(constDef->constName == "is_ref");
  REQUIRE(constDef->genericParams.size() == 1);
  REQUIRE(constDef->returnType != nullptr);
  REQUIRE(constDef->returnType->repr() == "bool");
  REQUIRE(constDef->native);

  destroyast(ast);
}

TEST_CASE("parser should parse const function declarations", "[Parser][Generics][ConstFun]")
{
  auto ast = parse("const fun is_even(value: i32) -> bool { return value % 2 == 0; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->constEval);
  REQUIRE(funDef->funName == "is_even");
  REQUIRE(funDef->params.size() == 1);
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->repr().starts_with("const fun is_even"));

  destroyast(ast);
}

TEST_CASE("parser should parse const predicate specializations", "[Parser][Generics][ConstPredicate]")
{
  auto ast = parse("const<T> is_ref<ref<T>>: bool = true;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto constDef = dynamic_ast_cast<ConstDef>(compileUnit->module->definitions[0]);
  REQUIRE(constDef != nullptr);
  REQUIRE(constDef->constName == "is_ref");
  REQUIRE(constDef->genericParams.size() == 1);
  REQUIRE(constDef->specializationPattern != nullptr);
  REQUIRE(constDef->specializationPattern->repr() == "is_ref<ref<T>>");
  REQUIRE(constDef->returnType != nullptr);
  REQUIRE(constDef->returnType->repr() == "bool");
  REQUIRE_FALSE(constDef->native);

  destroyast(ast);
}

TEST_CASE("parser should parse const specialization where predicates", "[Parser][Generics][ConstPredicate]")
{
  auto ast = parse("const<T> is_box<ref<T>> where is_ref<T>: bool = true;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto constDef = dynamic_ast_cast<ConstDef>(compileUnit->module->definitions[0]);
  REQUIRE(constDef != nullptr);
  REQUIRE(constDef->constName == "is_box");
  REQUIRE(constDef->specializationPattern != nullptr);
  REQUIRE(constDef->whereBounds.size() == 1);
  REQUIRE(constDef->whereBounds[0]->predicate != nullptr);
  REQUIRE(constDef->whereBounds[0]->predicate->repr() == "is_ref<T>");

  destroyast(ast);
}

TEST_CASE("parser should parse const specialization where trait constraints", "[Parser][Generics][ConstPredicate]")
{
  auto ast = parse("const<T> is_show_ref<ref<T>> where T: Show + Debug && is_ref<ref<T>>: bool = true;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto constDef = dynamic_ast_cast<ConstDef>(compileUnit->module->definitions[0]);
  REQUIRE(constDef != nullptr);
  REQUIRE(constDef->constName == "is_show_ref");
  REQUIRE(constDef->specializationPattern != nullptr);
  REQUIRE(constDef->whereBounds.size() == 3);
  REQUIRE(constDef->whereBounds[0]->repr() == "T: Show");
  REQUIRE(constDef->whereBounds[1]->repr() == "T: Debug");
  REQUIRE(constDef->whereBounds[2]->repr() == "is_ref<ref<T>>");

  destroyast(ast);
}

TEST_CASE("parser should parse generic const predicate where clauses", "[Parser][Generics][ConstPredicate]")
{
  auto ast = parse(R"(
    fun accept<T>(value: T) -> T where !is_ref<T> = value;
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->whereBounds.size() == 1);
  REQUIRE(funDef->whereBounds[0]->predicate != nullptr);
  REQUIRE(funDef->whereBounds[0]->predicate->repr() == "!is_ref<T>");

  destroyast(ast);
}

TEST_CASE("parser should parse const predicate where clauses before block bodies",
          "[Parser][Generics][ConstPredicate]")
{
  auto ast = parse(R"(
    fun accept<T>(value: T) -> unit where is_ref<T> {
      return;
    }
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->whereBounds.size() == 1);
  REQUIRE(funDef->whereBounds[0]->predicate != nullptr);
  REQUIRE(funDef->whereBounds[0]->predicate->repr() == "is_ref<T>");

  destroyast(ast);
}

// ============================================================================
// NewType with Generics
// ============================================================================

TEST_CASE("parser should parse newtype with generic param", "[Parser][Generics][NewType]")
{
  auto ast = parse("type UserId<T> wraps T;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto ntDef = dynamic_ast_cast<NewTypeDef>(compileUnit->module->definitions[0]);
  REQUIRE(ntDef != nullptr);
  REQUIRE(ntDef->typeName == "UserId");
  REQUIRE(ntDef->genericParams.size() == 1);
  REQUIRE(ntDef->genericParams[0]->name == "T");
  REQUIRE(ntDef->wrappedType != nullptr);
  REQUIRE(ntDef->wrappedType->name == "T");
  REQUIRE(ntDef->repr() == "type UserId<T> wraps T;");

  destroyast(ast);
}

TEST_CASE("parser should parse newtype with multiple generic params", "[Parser][Generics][NewType]")
{
  auto ast = parse("type Tagged<T, U> wraps (T, U);");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto ntDef = dynamic_ast_cast<NewTypeDef>(compileUnit->module->definitions[0]);
  REQUIRE(ntDef != nullptr);
  REQUIRE(ntDef->typeName == "Tagged");
  REQUIRE(ntDef->genericParams.size() == 2);
  REQUIRE(ntDef->genericParams[0]->name == "T");
  REQUIRE(ntDef->genericParams[1]->name == "U");

  destroyast(ast);
}

// ============================================================================
// Tagged Union with Generics
// ============================================================================

TEST_CASE("parser should parse tagged union with generic params", "[Parser][Generics][TaggedUnion]")
{
  auto ast = parse("type Result<T, E> = Ok(T) | Err(E);");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto tuDef = dynamic_ast_cast<TaggedUnionDef>(compileUnit->module->definitions[0]);
  REQUIRE(tuDef != nullptr);
  REQUIRE(tuDef->typeName == "Result");
  REQUIRE(tuDef->genericParams.size() == 2);
  REQUIRE(tuDef->genericParams[0]->name == "T");
  REQUIRE(tuDef->genericParams[1]->name == "E");
  REQUIRE(tuDef->variants.size() == 2);
  REQUIRE(tuDef->variants[0].variantName == "Ok");
  REQUIRE(tuDef->variants[1].variantName == "Err");

  destroyast(ast);
}

TEST_CASE("parser should parse tagged union with single generic param", "[Parser][Generics][TaggedUnion]")
{
  auto ast = parse("type Option<T> = Some(T) | None;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto tuDef = dynamic_ast_cast<TaggedUnionDef>(compileUnit->module->definitions[0]);
  REQUIRE(tuDef != nullptr);
  REQUIRE(tuDef->typeName == "Option");
  REQUIRE(tuDef->genericParams.size() == 1);
  REQUIRE(tuDef->genericParams[0]->name == "T");
  REQUIRE(tuDef->variants.size() == 2);
  REQUIRE(tuDef->variants[0].variantName == "Some");
  REQUIRE(tuDef->variants[0].payloadTypes.size() == 1);
  REQUIRE(tuDef->variants[1].variantName == "None");

  destroyast(ast);
}

TEST_CASE("parser should parse tagged union with generic and multiple payloads", "[Parser][Generics][TaggedUnion]")
{
  auto ast = parse("type Either<L, R> = Left(L, string) | Right(R);");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto tuDef = dynamic_ast_cast<TaggedUnionDef>(compileUnit->module->definitions[0]);
  REQUIRE(tuDef != nullptr);
  REQUIRE(tuDef->typeName == "Either");
  REQUIRE(tuDef->genericParams.size() == 2);
  REQUIRE(tuDef->genericParams[0]->name == "L");
  REQUIRE(tuDef->genericParams[1]->name == "R");
  REQUIRE(tuDef->variants.size() == 2);
  REQUIRE(tuDef->variants[0].variantName == "Left");
  REQUIRE(tuDef->variants[0].payloadTypes.size() == 2);
  REQUIRE(tuDef->variants[1].variantName == "Right");
  REQUIRE(tuDef->variants[1].payloadTypes.size() == 1);
  REQUIRE(tuDef->repr() == "type Either<L, R> = Left(L, string) | Right(R)");

  destroyast(ast);
}

TEST_CASE("parser should parse non-generic tagged union (no regression)", "[Parser][Generics][TaggedUnion]")
{
  auto ast = parse("type Color = Red | Green | Blue;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto tuDef = dynamic_ast_cast<TaggedUnionDef>(compileUnit->module->definitions[0]);
  REQUIRE(tuDef != nullptr);
  REQUIRE(tuDef->typeName == "Color");
  REQUIRE(tuDef->genericParams.size() == 0);
  REQUIRE(tuDef->variants.size() == 3);

  destroyast(ast);
}

// ============================================================================
// Type Annotations with Generic Args
// ============================================================================

TEST_CASE("parser should parse type annotation with generic args", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Option<int>) -> int { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "Option");
  REQUIRE(funDef->params[0]->annotatedType->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "int");

  destroyast(ast);
}

TEST_CASE("parser should parse type annotation with multiple generic args", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Map<string, int>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "Map");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 2);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "string");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[1]->name == "int");

  destroyast(ast);
}

TEST_CASE("parser should parse nested generic type annotations", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Option<Option<int>>) -> int { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params[0]->annotatedType != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "Option");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 1);
  auto inner = funDef->params[0]->annotatedType->genericArgs[0];
  REQUIRE(inner->name == "Option");
  REQUIRE(inner->genericArgs.size() == 1);
  REQUIRE(inner->genericArgs[0]->name == "int");

  destroyast(ast);
}

TEST_CASE("parser should parse triple nested generic type annotations", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Option<Option<Option<int>>>) -> int { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  auto outer = funDef->params[0]->annotatedType;
  REQUIRE(outer->name == "Option");
  REQUIRE(outer->genericArgs.size() == 1);

  auto middle = outer->genericArgs[0];
  REQUIRE(middle->name == "Option");
  REQUIRE(middle->genericArgs.size() == 1);

  auto inner = middle->genericArgs[0];
  REQUIRE(inner->name == "Option");
  REQUIRE(inner->genericArgs.size() == 1);
  REQUIRE(inner->genericArgs[0]->name == "int");

  destroyast(ast);
}

TEST_CASE("parser should parse generic type annotation in return type", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn() -> Option<int> { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->name == "Option");
  REQUIRE(funDef->returnType->genericArgs.size() == 1);
  REQUIRE(funDef->returnType->genericArgs[0]->name == "int");

  destroyast(ast);
}

TEST_CASE("parser should parse generic type annotation in val definition", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("val items: vector<int> = [1, 2, 3];");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);
  REQUIRE(valStmt->typeAnnotation->name == "vector");
  // `vector<int>` where `vector` is an identifier is parsed as CUSTOMIZED with genericArgs.
  REQUIRE(valStmt->typeAnnotation->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 1);
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->name == "int");

  destroyast(ast);
}

TEST_CASE("parser should parse generic args with builtin type arguments", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Option<i32>, y: Option<string>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params.size() == 2);

  REQUIRE(funDef->params[0]->annotatedType->name == "Option");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "i32");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->type == TypeAnnotationType::BUILTIN_I32);

  REQUIRE(funDef->params[1]->annotatedType->name == "Option");
  REQUIRE(funDef->params[1]->annotatedType->genericArgs.size() == 1);
  REQUIRE(funDef->params[1]->annotatedType->genericArgs[0]->name == "string");

  destroyast(ast);
}

TEST_CASE("parser should parse generic type annotation without generic args (no regression)",
          "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: MyType) -> MyType { return x; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params[0]->annotatedType != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "MyType");
  REQUIRE(funDef->params[0]->annotatedType->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 0);

  destroyast(ast);
}

TEST_CASE("parser should parse generic arg with unit type", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Result<unit, string>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "Result");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 2);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "unit");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->type == TypeAnnotationType::BUILTIN_UNIT);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[1]->name == "string");

  destroyast(ast);
}

TEST_CASE("parser should parse generic arg with bool type", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Option<bool>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "bool");

  destroyast(ast);
}

TEST_CASE("parser should parse generic arg with array type inside", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Option<[int]>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  auto optType = funDef->params[0]->annotatedType;
  REQUIRE(optType->name == "Option");
  REQUIRE(optType->genericArgs.size() == 1);
  REQUIRE(optType->genericArgs[0]->type == TypeAnnotationType::VECTOR);

  destroyast(ast);
}

TEST_CASE("parser should parse generic arg with tuple type inside", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Option<(int, string)>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  auto optType = funDef->params[0]->annotatedType;
  REQUIRE(optType->name == "Option");
  REQUIRE(optType->genericArgs.size() == 1);
  REQUIRE(optType->genericArgs[0]->type == TypeAnnotationType::TUPLE);
  REQUIRE(optType->genericArgs[0]->arguments.size() == 2);

  destroyast(ast);
}

TEST_CASE("parser should parse nested generics with different type names", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Result<Option<int>, string>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  auto resultType = funDef->params[0]->annotatedType;
  REQUIRE(resultType->name == "Result");
  REQUIRE(resultType->genericArgs.size() == 2);

  auto firstArg = resultType->genericArgs[0];
  REQUIRE(firstArg->name == "Option");
  REQUIRE(firstArg->genericArgs.size() == 1);
  REQUIRE(firstArg->genericArgs[0]->name == "int");

  auto secondArg = resultType->genericArgs[1];
  REQUIRE(secondArg->name == "string");
  REQUIRE(secondArg->genericArgs.size() == 0);

  destroyast(ast);
}

TEST_CASE("parser should parse deeply nested mixed generics", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun myfn(x: Map<string, Option<array<i32, 4>>>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  auto mapType = funDef->params[0]->annotatedType;
  REQUIRE(mapType->name == "Map");
  REQUIRE(mapType->genericArgs.size() == 2);

  REQUIRE(mapType->genericArgs[0]->name == "string");

  auto optType = mapType->genericArgs[1];
  REQUIRE(optType->name == "Option");
  REQUIRE(optType->genericArgs.size() == 1);

  auto arrType = optType->genericArgs[0];
  REQUIRE(arrType->name == "array");
  // `array<i32, 4>` uses fixed array generic syntax with a const literal argument.
  REQUIRE(arrType->type == TypeAnnotationType::CUSTOMIZED);
  REQUIRE(arrType->genericArgs.size() == 2);
  REQUIRE(arrType->genericArgs[1]->constLiteral);
  REQUIRE(arrType->genericArgs[0]->name == "i32");
  REQUIRE(arrType->genericArgs[0]->type == TypeAnnotationType::BUILTIN_I32);

  destroyast(ast);
}

// ============================================================================
// Error Cases — Invalid Generic Syntax
// ============================================================================

TEST_CASE("parser should reject generic params without name", "[Parser][Generics][Error]")
{
  parseInvalid("fun<> identity(x: int) -> int { return x; }", "");
  // This should either parse as empty generic or error — depends on design
  // The key is it should not crash
}

TEST_CASE("parser should reject unclosed generic params", "[Parser][Generics][Error]")
{
  parseInvalid("fun<T identity(x: T) -> T { return x; }", "");
  // Should produce parse error due to unclosed '<'
}

TEST_CASE("parser should reject unclosed generic args in type annotation", "[Parser][Generics][Error]")
{
  parseInvalid("fun myfn(x: Option<int) -> int { return x; }", "");
  // Should produce parse error due to unclosed '<' in type annotation
}

// ============================================================================
// Multiple Definitions with Generics
// ============================================================================

TEST_CASE("parser should parse multiple generic definitions in sequence", "[Parser][Generics]")
{
  auto ast = parse("fun<T> identity(x: T) -> T { return x; }\nfun<U, V> pair(a: U, b: V) -> U { return a; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module;
  REQUIRE(mod != nullptr);
  REQUIRE(mod->definitions.size() == 2);

  auto fun1 = dynamic_ast_cast<FunctionDef>(mod->definitions[0]);
  REQUIRE(fun1 != nullptr);
  REQUIRE(fun1->funName == "identity");
  REQUIRE(fun1->genericParams.size() == 1);
  REQUIRE(fun1->genericParams[0]->name == "T");

  auto fun2 = dynamic_ast_cast<FunctionDef>(mod->definitions[1]);
  REQUIRE(fun2 != nullptr);
  REQUIRE(fun2->funName == "pair");
  REQUIRE(fun2->genericParams.size() == 2);
  REQUIRE(fun2->genericParams[0]->name == "U");
  REQUIRE(fun2->genericParams[1]->name == "V");

  destroyast(ast);
}

TEST_CASE("parser should parse generic and non-generic definitions mixed", "[Parser][Generics]")
{
  auto ast = parse("fun simple(x: i32) -> i32 { return x; }\n"
                   "fun<T> identity(x: T) -> T { return x; }\n"
                   "val z: i32 = 42;\n"
                   "type Box<T> { property value: T; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module;
  REQUIRE(mod != nullptr);
  REQUIRE(mod->definitions.size() == 4);

  auto fun1 = dynamic_ast_cast<FunctionDef>(mod->definitions[0]);
  REQUIRE(fun1 != nullptr);
  REQUIRE(fun1->genericParams.size() == 0);

  auto fun2 = dynamic_ast_cast<FunctionDef>(mod->definitions[1]);
  REQUIRE(fun2 != nullptr);
  REQUIRE(fun2->genericParams.size() == 1);

  auto valDef = dynamic_ast_cast<ValDef>(mod->definitions[2]);
  REQUIRE(valDef != nullptr);

  auto typeDef = dynamic_ast_cast<TypeDef>(mod->definitions[3]);
  REQUIRE(typeDef != nullptr);
  REQUIRE(typeDef->genericParams.size() == 1);

  destroyast(ast);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("parser should parse generic type arg with same name as outer type", "[Parser][Generics][TypeAnnotation]")
{
  // Unusual but valid: Option<Option>
  auto ast = parse("fun myfn(x: Option<Option>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->name == "Option");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "Option");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->genericArgs.size() == 0);

  destroyast(ast);
}

TEST_CASE("parser should parse generic params with underscores in names", "[Parser][Generics]")
{
  auto ast = parse("fun<T_val, U_val> process(a: T_val, b: U_val) -> T_val { return a; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 2);
  REQUIRE(funDef->genericParams[0]->name == "T_val");
  REQUIRE(funDef->genericParams[1]->name == "U_val");

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with all builtin return type", "[Parser][Generics][TypeAnnotation]")
{
  auto ast = parse("fun<T> len(items: T) -> i32 { return 0; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType->name == "T");
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->type == TypeAnnotationType::BUILTIN_I32);

  destroyast(ast);
}

TEST_CASE("parser should parse generic function with generic param used in multiple places", "[Parser][Generics]")
{
  auto ast = parse("fun<T> dup(x: T, y: T) -> (T, T) { return (x, y); }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->params.size() == 2);
  REQUIRE(funDef->params[0]->annotatedType->name == "T");
  REQUIRE(funDef->params[1]->annotatedType->name == "T");
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->type == TypeAnnotationType::TUPLE);
  REQUIRE(funDef->returnType->arguments.size() == 2);
  auto tupleArg0 = dynamic_ast_cast<TypeAnnotation>(funDef->returnType->arguments[0]);
  auto tupleArg1 = dynamic_ast_cast<TypeAnnotation>(funDef->returnType->arguments[1]);
  REQUIRE(tupleArg0 != nullptr);
  REQUIRE(tupleArg1 != nullptr);
  REQUIRE(tupleArg0->name == "T");
  REQUIRE(tupleArg1->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse generic type in new expression context", "[Parser][Generics][TypeAnnotation]")
{
  // Ensure generic types can appear in new object expressions type position
  auto ast = parse(
      "type Box<T> { property value: T; }\nfun make_box<T>(v: T) -> ref<Box<T>> { return new Box<T> { value: v }; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module;
  REQUIRE(mod->definitions.size() == 2);

  auto typeDef = dynamic_ast_cast<TypeDef>(mod->definitions[0]);
  REQUIRE(typeDef != nullptr);
  REQUIRE(typeDef->genericParams.size() == 1);

  auto funDef = dynamic_ast_cast<FunctionDef>(mod->definitions[1]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->name == "ref");
  REQUIRE(funDef->returnType->genericArgs.size() == 1);
  auto innerReturnType = dynamic_ast_cast<TypeAnnotation>(funDef->returnType->genericArgs[0]);
  REQUIRE(innerReturnType != nullptr);
  REQUIRE(innerReturnType->name == "Box");
  REQUIRE(innerReturnType->genericArgs.size() == 1);
  REQUIRE(innerReturnType->genericArgs[0]->name == "T");

  auto body = dynamic_ast_cast<CompoundStatement>(funDef->body);
  REQUIRE(body != nullptr);
  REQUIRE(body->statements.size() == 1);
  auto returnStmt = dynamic_ast_cast<ReturnStatement>(body->statements[0]);
  REQUIRE(returnStmt != nullptr);
  auto newExpr = dynamic_ast_cast<NewObjectExpression>(returnStmt->expression);
  REQUIRE(newExpr != nullptr);
  REQUIRE(newExpr->targetType != nullptr);
  REQUIRE(newExpr->targetType->name == "Box");
  REQUIRE(newExpr->targetType->genericArgs.size() == 1);
  REQUIRE(newExpr->targetType->genericArgs[0]->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse typeof expression with property access", "[Parser][Generics][TypeQuery]")
{
  auto ast = parse("val kind = typeof(1).kind;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module->definitions.size() == 1);

  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[0]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  auto accessor = dynamic_ast_cast<IdAccessorExpression>(valStmt->value);
  REQUIRE(accessor != nullptr);
  auto typeofExpr = dynamic_ast_cast<TypeOfExpression>(accessor->primaryExpression);
  REQUIRE(typeofExpr != nullptr);
  REQUIRE(typeofExpr->repr() == "typeof(1)");
  REQUIRE(accessor->accessor->repr() == "kind");

  destroyast(ast);
}

TEST_CASE("parser should recognize ref as nested generic argument starter", "[Parser][Generics][RefMove]")
{
  auto ast = parse("type Box<T> { property value: T; }\nval nested: Box<ref<i32>> = unit;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[1]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  REQUIRE(valStmt->typeAnnotation != nullptr);
  REQUIRE(valStmt->typeAnnotation->name == "Box");
  REQUIRE(valStmt->typeAnnotation->genericArgs.size() == 1);
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->name == "ref");
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->genericArgs.size() == 1);
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->genericArgs[0]->name == "i32");
  REQUIRE(valStmt->typeAnnotation->genericArgs[0]->genericArgs[0]->type == TypeAnnotationType::BUILTIN_I32);

  destroyast(ast);
}

TEST_CASE("parser should parse higher-kinded generic parameter placeholders", "[Parser][Generics][HKT]")
{
  auto ast = parse("fun accept_hkt<F<_>, T>(value: F<T>) -> unit { return unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 2);
  REQUIRE(funDef->genericParams[0]->name == "F");
  REQUIRE(funDef->genericParams[0]->kindArity == 1);
  REQUIRE(funDef->genericParams[1]->name == "T");
  REQUIRE(funDef->genericParams[1]->kindArity == 0);
  REQUIRE(funDef->params[0]->annotatedType->name == "F");
  REQUIRE(funDef->params[0]->annotatedType->genericArgs.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType->genericArgs[0]->name == "T");

  destroyast(ast);
}

TEST_CASE("parser should parse higher-kinded trait parameters", "[Parser][Generics][HKT]")
{
  auto ast = parse("trait Uses<F<_>, T> { fun accept(self: ref<Self>, value: F<T>) -> unit; }");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto traitDef = dynamic_ast_cast<TraitDef>(compileUnit->module->definitions[0]);
  REQUIRE(traitDef != nullptr);
  REQUIRE(traitDef->genericParams.size() == 2);
  REQUIRE(traitDef->genericParams[0]->name == "F");
  REQUIRE(traitDef->genericParams[0]->kindArity == 1);
  REQUIRE(traitDef->genericParams[1]->name == "T");
  REQUIRE(traitDef->genericParams[1]->kindArity == 0);

  destroyast(ast);
}

TEST_CASE("parser should parse variadic higher-kinded parameter placeholders", "[Parser][Generics][HKT][Pack]")
{
  auto ast = parse("fun accept_hkt<F<_, ...>, T>(value: F<T>) -> unit = unit;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 2);
  REQUIRE(funDef->genericParams[0]->name == "F");
  REQUIRE(funDef->genericParams[0]->kindArity == 1);
  REQUIRE(funDef->genericParams[0]->kindVariadicTail);

  destroyast(ast);
}

TEST_CASE("parser should parse pack-only higher-kinded parameter placeholders", "[Parser][Generics][HKT][Pack]")
{
  auto ast = parse("fun accept_hkt<F<...>>() -> unit = unit;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);
  REQUIRE(funDef->genericParams[0]->name == "F");
  REQUIRE(funDef->genericParams[0]->kindArity == 0);
  REQUIRE(funDef->genericParams[0]->kindVariadicTail);

  destroyast(ast);
}

TEST_CASE("parser should reject non-final variadic kind placeholder", "[Parser][Generics][HKT][Pack][Failure]")
{
  auto ast = parseInvalid("fun bad<F<..., _>>() -> unit = unit;",
                          "Variadic kind placeholder must be the final placeholder");
  REQUIRE(ast == nullptr);
}

TEST_CASE("parser should reject duplicate variadic kind placeholders", "[Parser][Generics][HKT][Pack][Failure]")
{
  auto ast = parseInvalid("fun bad<F<_, ..., ...>>() -> unit = unit;",
                          "Variadic kind placeholder must be the final placeholder");
  REQUIRE(ast == nullptr);
}

TEST_CASE("parser should reject bare kind placeholder as a normal type annotation", "[Parser][Generics][HKT][Failure]")
{
  auto ast = parseInvalid("fun bad(value: _) -> unit { return unit; }",
                          "Type placeholder '_' is only allowed in generic parameter kind declarations");
  REQUIRE(ast == nullptr);
}
