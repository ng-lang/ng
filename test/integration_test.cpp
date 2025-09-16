
#include "test.hpp"
#include <intp/intp.hpp>
#include <filesystem>
#include <fstream>

using namespace NG::intp;

namespace fs = std::filesystem;

static inline void
runIntegrationTest(const std::string &filename)
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

TEST_CASE("should run with id function definition", "[Integration]")
{
    runIntegrationTest("example/01.id.ng");
}

TEST_CASE("should run with many definitions", "[Integration]")
{
    runIntegrationTest("example/02.many_defs.ng");
}

TEST_CASE("should run with function calls", "[Integration]")
{
    runIntegrationTest("example/03.funcall_and_idexpr.ng");
}

TEST_CASE("should run with string concat", "[Integration]")
{
    runIntegrationTest("example/04.str.ng");
}

TEST_CASE("should run with valdefs -- function and members", "[Integration]")
{
    runIntegrationTest("example/05.valdef.ng");
}

TEST_CASE("should run with type definition and object creation", "[Integration]")
{
    runIntegrationTest("example/07.object.ng");
}

TEST_CASE("should run with array", "[Integration]")
{
    runIntegrationTest("example/06.array.ng");
}

TEST_CASE("should run with multiple modules import and export", "[Integration]")
{
    runIntegrationTest("example/08.imports.ng");
}

TEST_CASE("should run with multi level scopes", "[Integration]")
{
    runIntegrationTest("example/09.scope.ng");
}

TEST_CASE("should run with basic single variable loop", "[IntegrationLoop]")
{
    runIntegrationTest("example/10.loop.ng");
}

TEST_CASE("should run with customized iterator", "[IntegrationLoop]")
{
    runIntegrationTest("example/11.iterator_example.ng");
}

TEST_CASE("should run with max loop stack", "[IntegrationLoop]")
{
    // todo: fix possible stack overflow.
    // runIntegrationTest("example/12.loop_max_stack.ng");
}

TEST_CASE("should run prelude as default import module", "[IntegrationLoop]")
{
    runIntegrationTest("example/13.import_std_prelude.ng");
}
