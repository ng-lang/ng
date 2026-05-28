
#include "test.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <intp/intp.hpp>
#include <module.hpp>
#include <typecheck/typecheck.hpp>
#include <vector>

using namespace NG::intp;

namespace fs = std::filesystem;

namespace
{
struct ScopedEnvVar
{
  Str name;
  Str previous;
  bool hadPrevious = false;

  ScopedEnvVar(Str name, const Str &value) : name(std::move(name))
  {
    if (const char *existing = std::getenv(this->name.c_str()))
    {
      previous = existing;
      hadPrevious = true;
    }
#ifdef _WIN32
    _putenv_s(this->name.c_str(), value.c_str());
#else
    setenv(this->name.c_str(), value.c_str(), 1);
#endif
  }

  ~ScopedEnvVar()
  {
#ifdef _WIN32
    _putenv_s(name.c_str(), hadPrevious ? previous.c_str() : "");
#else
    if (hadPrevious)
    {
      setenv(name.c_str(), previous.c_str(), 1);
    }
    else
    {
      unsetenv(name.c_str());
    }
#endif
  }
};

struct SourceModuleFixture
{
  fs::path root;
  ScopedEnvVar modulePath;

  SourceModuleFixture()
      : root(fs::temp_directory_path() /
             ("ng_stupid_module_artifact_" +
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))),
        modulePath("NG_MODULE_PATH", root.string())
  {
    NG::module::clear_module_loader_cache();
    NG::module::get_module_registry().clear();
    fs::create_directories(root);
  }

  ~SourceModuleFixture()
  {
    NG::module::clear_module_loader_cache();
    NG::module::get_module_registry().clear();
    fs::remove_all(root);
  }

  void write(const fs::path &relative, const Str &source) const
  {
    auto target = root / relative;
    fs::create_directories(target.parent_path());
    std::ofstream out{target};
    REQUIRE(out.good());
    out << source;
  }
};
} // namespace

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
      "example/52.const_array_vector_span.ng",
      "example/56.stdlib_modules.ng",
      "example/57.ranges_slicing_pipeline.ng",
      "example/58.fold_expressions.ng",
      "example/59.std_list_sequence.ng",
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

TEST_CASE("should run auto derive trait example with STUPID", "[Integration][AutoDerive]")
{
  runTypecheckedIntegrationTest("example/55.auto_derive_traits.ng");
}

TEST_CASE("STUPID should import source module through canonical module id",
          "[Integration][ModuleArtifact]")
{
  SourceModuleFixture fixture;
  fixture.write("pkg/math.ng", R"(
    module pkg.math exports *;
    fun answer() -> i32 {
      return 42;
    }
  )");

  auto ast = parse(R"(
    import pkg.math (*);
    assert(answer() == 42);
  )");
  REQUIRE(ast != nullptr);

  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, preludeTypes, {"[force-module-loader]"});

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);

  delete intp;
  destroyast(ast);
}

TEST_CASE("STUPID should reject conflicting imported symbols from different modules",
          "[Integration][ModuleArtifact]")
{
  SourceModuleFixture fixture;
  fixture.write("pkg/alpha.ng", R"(
    module pkg.alpha exports *;
    fun duplicated() -> i32 {
      return 1;
    }
  )");
  fixture.write("pkg/beta.ng", R"(
    module pkg.beta exports *;
    fun duplicated() -> i32 {
      return 2;
    }
  )");

  auto ast = parse(R"(
    import pkg.alpha (*);
    import pkg.beta (*);
  )");
  REQUIRE(ast != nullptr);

  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, preludeTypes, {"[force-module-loader]"});

  Interpreter *intp = NG::intp::stupid();
  REQUIRE_THROWS_WITH(ast->accept(intp), Catch::Matchers::ContainsSubstring("Import conflict for symbol: duplicated"));

  delete intp;
  destroyast(ast);
}

TEST_CASE("STUPID should allow qualified imports to avoid symbol conflicts",
          "[Integration][ModuleArtifact]")
{
  SourceModuleFixture fixture;
  fixture.write("pkg/alpha.ng", R"(
    module pkg.alpha exports *;
    fun duplicated() -> i32 {
      return 1;
    }
  )");
  fixture.write("pkg/beta.ng", R"(
    module pkg.beta exports *;
    fun duplicated() -> i32 {
      return 2;
    }
  )");

  auto ast = parse(R"(
    import pkg.alpha as first;
    import pkg.beta;
    assert(first.duplicated() == 1);
    assert(beta.duplicated() == 2);
  )");
  REQUIRE(ast != nullptr);

  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, preludeTypes, {"[force-module-loader]"});

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);

  delete intp;
  destroyast(ast);
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

TEST_CASE("should run const fun example with STUPID", "[Integration][ConstFun]")
{
  runTypecheckedIntegrationTest("example/53.const_fun.ng");
}

TEST_CASE("should run enhanced tuple type example with STUPID", "[Integration][Tuple][EnhancedTuple]")
{
  runTypecheckedIntegrationTest("example/54.enhanced_tuple_types.ng");
}

TEST_CASE("should run higher-kinded generic example with STUPID", "[Integration][HKT]")
{
  runTypecheckedIntegrationTest("example/48.higher_kinded_generics.ng");
}

TEST_CASE("should run variadic higher-kinded generic example with STUPID", "[Integration][HKT][Pack]")
{
  runTypecheckedIntegrationTest("example/49.variadic_hkt_kind.ng");
}
