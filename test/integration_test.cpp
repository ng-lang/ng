
#include "test.hpp"
#include <filesystem>
#include <fstream>
#include <intp/intp.hpp>
#include <typecheck/typecheck.hpp>
#include <vector>

using namespace NG::intp;

namespace fs = std::filesystem;

static inline void runIntegrationTest(const std::string &filename)
{
  std::string target = filename;
  fs::path cwd = std::filesystem::current_path();
  if (!fs::is_directory(cwd / "example"))
  {
    target = "../" + filename;
  }
  debug_log("Running " + target);
  std::ifstream file(target);
  std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  auto ast = parse(source, target);

  REQUIRE(ast != nullptr);

  Interpreter *intp = NG::intp::stupid();

  ast->accept(intp);

  // intp->summary();

  delete intp;
  destroyast(ast);
}

static inline void runTypecheckedIntegrationTest(const std::string &filename)
{
  std::string target = filename;
  fs::path cwd = std::filesystem::current_path();
  if (!fs::is_directory(cwd / "example"))
  {
    target = "../" + filename;
  }
  debug_log("Running typechecked " + target);
  std::ifstream file(target);
  std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  auto ast = parse(source, target);

  REQUIRE(ast != nullptr);

  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, preludeTypes);

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);

  delete intp;
  destroyast(ast);
}

TEST_CASE("should run with max loop stack", "[IntegrationLoop]")
{
  runIntegrationTest("example/12.loop_max_stack.ng");
}

TEST_CASE("should run numbered examples", "[Integration]")
{
  const std::vector<std::string> examples = {
      "example/01.id.ng",
      "example/02.many_defs.ng",
      "example/03.funcall_and_idexpr.ng",
      "example/04.str.ng",
      "example/05.valdef.ng",
      "example/06.array.ng",
      "example/07.object.ng",
      "example/08.imports.ng",
      "example/09.scope.ng",
      "example/10.loop.ng",
      "example/11.iterator_example.ng",
      "example/13.import_std_prelude.ng",
      "example/14.tuple.ng",
      "example/15.generics.ng",
      "example/16.tagged_union.ng",
      "example/17.const_if.ng",
      "example/18.stdlib_basics.ng",
      "example/19.union_type.ng",
      "example/20.switch_otherwise.ng",
      "example/21.recursive_tagged_union_ref.ng",
      "example/22.ref_move_swap.ng",
      "example/23.ref_places.ng",
      "example/24.move_value_semantics.ng",
      "example/25.trait_show.ng",
      "example/26.trait_generic_bound.ng",
      "example/27.trait_receiver_ref.ng",
      "example/28.trait_supertraits.ng",
      "example/29.trait_qualified_call.ng",
      "example/30.trait_inherent_precedence.ng",
      "example/31.trait_default_methods.ng",
      "example/32.trait_default_override.ng",
      "example/33.trait_default_supertraits.ng",
      "example/34.trait_object_show.ng",
      "example/35.trait_object_default.ng",
      "example/36.trait_object_mutation.ng",
      "example/37.copy_marker.ng",
      "example/38.clone_trait.ng",
      "example/39.drop_raii.ng",
      "example/40.trait_object_list.ng",
      "example/41.drop_smart_pointer.ng",
      "example/50.partial_move.ng",
      "example/51.partial_move_drop.ng",
  };

  for (const auto &example : examples)
  {
    runIntegrationTest(example);
  }
}

TEST_CASE("should parse and run shebang example source", "[Integration][Shebang]")
{
  runIntegrationTest("example/shebang.ng");
}

TEST_CASE("should run const type predicate example with STUPID", "[Integration][ConstPredicate]")
{
  runTypecheckedIntegrationTest("example/42.const_type_predicate.ng");
}

TEST_CASE("should run const specialization example with STUPID", "[Integration][ConstPredicate]")
{
  runTypecheckedIntegrationTest("example/43.const_specialization.ng");
}

TEST_CASE("should run type specialization example with STUPID", "[Integration][TypeSpecialization]")
{
  runTypecheckedIntegrationTest("example/44.type_specialization.ng");
}

TEST_CASE("should run native constraints example with STUPID", "[Integration][ConstPredicate]")
{
  runTypecheckedIntegrationTest("example/45.native_constraints.ng");
}

TEST_CASE("should run const trait constraints example with STUPID", "[Integration][ConstPredicate]")
{
  runTypecheckedIntegrationTest("example/46.const_trait_constraints.ng");
}

TEST_CASE("should run const generic instances example with STUPID", "[Integration][ConstPredicate]")
{
  runTypecheckedIntegrationTest("example/47.const_generic_instances.ng");
}

TEST_CASE("should run higher-kinded generic example with STUPID", "[Integration][HKT]")
{
  runTypecheckedIntegrationTest("example/48.higher_kinded_generics.ng");
}

TEST_CASE("should run variadic higher-kinded generic example with STUPID", "[Integration][HKT][Pack]")
{
  runTypecheckedIntegrationTest("example/49.variadic_hkt_kind.ng");
}
