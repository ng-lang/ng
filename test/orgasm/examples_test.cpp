#include "../test.hpp"
#include <filesystem>
#include <fstream>
#include <orgasm/compiler.hpp>
#include <orgasm/vm.hpp>
#include <module.hpp>

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

    NG::module::clear_module_loader_cache();
    NG::module::get_module_registry().clear();

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

TEST_CASE("Orgasm example 01.id.ng", "[OrgasmExample]") { runOrgasmExample("example/01.id.ng"); }
TEST_CASE("Orgasm example 02.many_defs.ng", "[OrgasmExample]") { runOrgasmExample("example/02.many_defs.ng"); }
TEST_CASE("Orgasm example 03.funcall_and_idexpr.ng", "[OrgasmExample]") { runOrgasmExample("example/03.funcall_and_idexpr.ng"); }
TEST_CASE("Orgasm example 04.str.ng", "[OrgasmExample]") { runOrgasmExample("example/04.str.ng"); }
TEST_CASE("Orgasm example 05.valdef.ng", "[OrgasmExample]") { runOrgasmExample("example/05.valdef.ng"); }
TEST_CASE("Orgasm example 06.array.ng", "[OrgasmExample]") { runOrgasmExample("example/06.array.ng"); }
TEST_CASE("Orgasm example 07.object.ng", "[OrgasmExample]") { runOrgasmExample("example/07.object.ng"); }
TEST_CASE("Orgasm example 08.imports.ng", "[OrgasmExample]") { runOrgasmExample("example/08.imports.ng"); }
TEST_CASE("Orgasm example 09.scope.ng", "[OrgasmExample]") { runOrgasmExample("example/09.scope.ng"); }
TEST_CASE("Orgasm example 10.loop.ng", "[OrgasmExample]") { runOrgasmExample("example/10.loop.ng"); }
TEST_CASE("Orgasm example 11.iterator_example.ng", "[OrgasmExample]") { runOrgasmExample("example/11.iterator_example.ng"); }
TEST_CASE("Orgasm example 13.import_std_prelude.ng", "[OrgasmExample]") { runOrgasmExample("example/13.import_std_prelude.ng"); }
TEST_CASE("Orgasm example 14.tuple.ng", "[OrgasmExample]") { runOrgasmExample("example/14.tuple.ng"); }
TEST_CASE("Orgasm example 16.tagged_union.ng", "[OrgasmExample]") { runOrgasmExample("example/16.tagged_union.ng"); }
TEST_CASE("Orgasm example 17.const_if.ng", "[OrgasmExample]") { runOrgasmExample("example/17.const_if.ng"); }
TEST_CASE("Orgasm example 19.union_type.ng", "[OrgasmExample]") { runOrgasmExample("example/19.union_type.ng"); }
TEST_CASE("Orgasm example 20.switch_otherwise.ng", "[OrgasmExample]") { runOrgasmExample("example/20.switch_otherwise.ng"); }
TEST_CASE("Orgasm example 21.recursive_tagged_union_ref.ng", "[OrgasmExample]") { runOrgasmExample("example/21.recursive_tagged_union_ref.ng"); }
