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
