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

    auto nativeNames = NG::library::prelude::native_function_names();
    Compiler compiler(modulePaths, nativeNames);
    auto bytecode = compiler.compile(dynamic_ast_cast<ast::CompileUnit>(ast));

    VM vm(modulePaths);
    NG::library::prelude::register_vm_natives(vm);
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
TEST_CASE("Orgasm example 22.ref_move_swap.ng", "[OrgasmExample]") { runOrgasmExample("example/22.ref_move_swap.ng"); }
TEST_CASE("Orgasm example 23.ref_places.ng", "[OrgasmExample]") { runOrgasmExample("example/23.ref_places.ng"); }
TEST_CASE("Orgasm example 24.move_value_semantics.ng", "[OrgasmExample]") { runOrgasmExample("example/24.move_value_semantics.ng"); }
TEST_CASE("Orgasm example 25.trait_show.ng", "[OrgasmExample]") { runOrgasmExample("example/25.trait_show.ng"); }
TEST_CASE("Orgasm example 26.trait_generic_bound.ng", "[OrgasmExample]") { runOrgasmExample("example/26.trait_generic_bound.ng"); }
TEST_CASE("Orgasm example 27.trait_receiver_ref.ng", "[OrgasmExample]") { runOrgasmExample("example/27.trait_receiver_ref.ng"); }
TEST_CASE("Orgasm example 28.trait_supertraits.ng", "[OrgasmExample]") { runOrgasmExample("example/28.trait_supertraits.ng"); }
TEST_CASE("Orgasm example 29.trait_qualified_call.ng", "[OrgasmExample]") { runOrgasmExample("example/29.trait_qualified_call.ng"); }
TEST_CASE("Orgasm example 30.trait_inherent_precedence.ng", "[OrgasmExample]") { runOrgasmExample("example/30.trait_inherent_precedence.ng"); }
TEST_CASE("Orgasm example 31.trait_default_methods.ng", "[OrgasmExample]") { runOrgasmExample("example/31.trait_default_methods.ng"); }
TEST_CASE("Orgasm example 32.trait_default_override.ng", "[OrgasmExample]") { runOrgasmExample("example/32.trait_default_override.ng"); }
TEST_CASE("Orgasm example 33.trait_default_supertraits.ng", "[OrgasmExample]") { runOrgasmExample("example/33.trait_default_supertraits.ng"); }
TEST_CASE("Orgasm example 34.trait_object_show.ng", "[OrgasmExample]") { runOrgasmExample("example/34.trait_object_show.ng"); }
TEST_CASE("Orgasm example 35.trait_object_default.ng", "[OrgasmExample]") { runOrgasmExample("example/35.trait_object_default.ng"); }
TEST_CASE("Orgasm example 36.trait_object_mutation.ng", "[OrgasmExample]") { runOrgasmExample("example/36.trait_object_mutation.ng"); }
TEST_CASE("Orgasm example 37.copy_marker.ng", "[OrgasmExample]") { runOrgasmExample("example/37.copy_marker.ng"); }
TEST_CASE("Orgasm example 38.clone_trait.ng", "[OrgasmExample]") { runOrgasmExample("example/38.clone_trait.ng"); }
TEST_CASE("Orgasm example 39.drop_raii.ng", "[OrgasmExample]") { runOrgasmExample("example/39.drop_raii.ng"); }
TEST_CASE("Orgasm example 40.trait_object_list.ng", "[OrgasmExample]") { runOrgasmExample("example/40.trait_object_list.ng"); }
TEST_CASE("Orgasm example 41.drop_smart_pointer.ng", "[OrgasmExample]") { runOrgasmExample("example/41.drop_smart_pointer.ng"); }
TEST_CASE("Orgasm example 42.const_type_predicate.ng", "[OrgasmExample]") { runOrgasmExample("example/42.const_type_predicate.ng"); }
TEST_CASE("Orgasm example 43.const_specialization.ng", "[OrgasmExample]") { runOrgasmExample("example/43.const_specialization.ng"); }
TEST_CASE("Orgasm example 44.type_specialization.ng", "[OrgasmExample]") { runOrgasmExample("example/44.type_specialization.ng"); }
TEST_CASE("Orgasm example 45.native_constraints.ng", "[OrgasmExample]") { runOrgasmExample("example/45.native_constraints.ng"); }
TEST_CASE("Orgasm example 46.const_trait_constraints.ng", "[OrgasmExample]") { runOrgasmExample("example/46.const_trait_constraints.ng"); }
TEST_CASE("Orgasm example 47.const_generic_instances.ng", "[OrgasmExample]") { runOrgasmExample("example/47.const_generic_instances.ng"); }
TEST_CASE("Orgasm example 48.higher_kinded_generics.ng", "[OrgasmExample]") { runOrgasmExample("example/48.higher_kinded_generics.ng"); }
