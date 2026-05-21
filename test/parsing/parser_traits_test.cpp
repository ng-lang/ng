#include "../test.hpp"

TEST_CASE("parser should parse trait declarations", "[Parser][Traits]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module->definitions.size() == 1);
  auto trait = dynamic_ast_cast<TraitDef>(compileUnit->module->definitions[0]);
  REQUIRE(trait != nullptr);
  REQUIRE(trait->traitName == "Show");
  REQUIRE(trait->methods.size() == 1);
  REQUIRE(trait->methods[0]->funName == "show");
  REQUIRE(trait->methods[0]->body == nullptr);

  destroyast(ast);
}

TEST_CASE("parser should parse impl and where bounds", "[Parser][Traits]")
{
  auto ast = parse(R"(
    impl<T> Show for Box<T> where T: Show {
      fun show(self: ref<Self>) -> string {
        return self.value.show();
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto impl = dynamic_ast_cast<ImplDef>(compileUnit->module->definitions[0]);
  REQUIRE(impl != nullptr);
  REQUIRE(impl->genericParams.size() == 1);
  REQUIRE(impl->trait->repr() == "Show");
  REQUIRE(impl->targetType->repr() == "Box<T>");
  REQUIRE(impl->whereBounds.size() == 1);
  REQUIRE(impl->methods.size() == 1);

  destroyast(ast);
}

TEST_CASE("parser should parse supertraits and qualified trait calls", "[Parser][Traits]")
{
  auto ast = parse(R"(
    trait Read {
      fun read(self: ref<Self>) -> string;
    }

    trait ReadWrite: Read {
      fun write(self: ref<Self>, value: string) -> unit;
    }

    val x = value.Read::read();
    val y = Read::read(ref value);
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto trait = dynamic_ast_cast<TraitDef>(compileUnit->module->definitions[1]);
  REQUIRE(trait != nullptr);
  REQUIRE(trait->traitName == "ReadWrite");
  REQUIRE(trait->superTraits.size() == 1);
  REQUIRE(trait->superTraits[0]->repr() == "Read");

  auto qualifiedReceiver = dynamic_ast_cast<QualifiedTraitCallExpression>(
      dynamic_ast_cast<ValDefStatement>(
          dynamic_ast_cast<ValDef>(compileUnit->module->definitions[2])->body)
          ->value);
  REQUIRE(qualifiedReceiver != nullptr);
  REQUIRE(qualifiedReceiver->receiver != nullptr);
  REQUIRE(qualifiedReceiver->traitName == "Read");
  REQUIRE(qualifiedReceiver->methodName == "read");

  auto qualifiedUfcs = dynamic_ast_cast<QualifiedTraitCallExpression>(
      dynamic_ast_cast<ValDefStatement>(
          dynamic_ast_cast<ValDef>(compileUnit->module->definitions[3])->body)
          ->value);
  REQUIRE(qualifiedUfcs != nullptr);
  REQUIRE(qualifiedUfcs->receiver == nullptr);
  REQUIRE(qualifiedUfcs->traitName == "Read");
  REQUIRE(qualifiedUfcs->methodName == "read");
  REQUIRE(qualifiedUfcs->arguments.size() == 1);

  destroyast(ast);
}

TEST_CASE("parser should parse trait default method bodies", "[Parser][Traits]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;

      fun bracketed(self: ref<Self>) -> string {
        return "[" + self.show() + "]";
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto trait = dynamic_ast_cast<TraitDef>(compileUnit->module->definitions[0]);
  REQUIRE(trait != nullptr);
  REQUIRE(trait->methods.size() == 2);
  REQUIRE(trait->methods[0]->body == nullptr);
  REQUIRE(trait->methods[1]->body != nullptr);

  destroyast(ast);
}

TEST_CASE("parser should parse explicit impl selection", "[Parser][Traits][UseImpl]")
{
  auto ast = parse("use impl Show for Box<i32>;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module->definitions.size() == 1);
  auto useImpl = dynamic_ast_cast<UseImplDecl>(compileUnit->module->definitions[0]);
  REQUIRE(useImpl != nullptr);
  REQUIRE(useImpl->trait->repr() == "Show");
  REQUIRE(useImpl->targetType->repr() == "Box<i32>");

  destroyast(ast);
}

TEST_CASE("parser should parse module-qualified explicit impl selection", "[Parser][Traits][UseImpl]")
{
  auto ast = parse("use impl impls::Show for Box<i32>;");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto useImpl = dynamic_ast_cast<UseImplDecl>(compileUnit->module->definitions[0]);
  REQUIRE(useImpl != nullptr);
  REQUIRE(useImpl->moduleQualifier == "impls");
  REQUIRE(useImpl->trait->repr() == "Show");
  REQUIRE(useImpl->targetType->repr() == "Box<i32>");

  destroyast(ast);
}

TEST_CASE("parser should export impl declarations", "[Parser][Traits][Export]")
{
  auto ast = parse(R"(
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "box";
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module->exports.size() == 1);
  REQUIRE(compileUnit->module->exports[0] == "impl Show for Box");

  destroyast(ast);
}
