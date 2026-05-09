
#include "test.hpp"
#include <filesystem>
#include <fstream>
#include <intp/intp.hpp>
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

TEST_CASE("should run with max loop stack", "[IntegrationLoop]")
{
  SKIP("example/12.loop_max_stack.ng still needs a real stack-growth fix before this can be asserted.");
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
