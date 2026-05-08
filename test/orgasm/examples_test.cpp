#include "../test.hpp"
#include <filesystem>
#include <fstream>
#include <orgasm/compiler.hpp>
#include <orgasm/vm.hpp>
#include <module.hpp>
#include <vector>

using namespace NG;
using namespace NG::orgasm;
namespace fs = std::filesystem;

static inline void runOrgasmExample(const std::string &filename)
{
    std::string target = filename;
    fs::path cwd = std::filesystem::current_path();
    fs::path project_root = cwd;
    
    if (!fs::is_directory(cwd / "example"))
    {
        target = "../" + filename;
        project_root = cwd.parent_path();
    }
    
    debug_log("Running Orgasm " + target);
    std::ifstream file(target);
    std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    auto ast = parse(source, target);

    REQUIRE(ast != nullptr);

    // Setup module paths
    Vec<Str> modulePaths;
    modulePaths.push_back((project_root / "lib").string());
    modulePaths.push_back((project_root / "example").string());

    Compiler compiler(modulePaths);
    auto bytecode = compiler.compile(dynamic_ast_cast<ast::CompileUnit>(ast));

    VM vm(modulePaths);
    vm.run(bytecode);

    destroyast(ast);
}

TEST_CASE("Orgasm should run supported numbered examples", "[OrgasmExample]")
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
        "example/16.tagged_union.ng",
        "example/17.const_if.ng",
        "example/19.union_type.ng",
        "example/20.switch_otherwise.ng",
    };

    for (const auto &example : examples)
    {
        runOrgasmExample(example);
    }
}
