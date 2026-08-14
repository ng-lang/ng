cmake_minimum_required(VERSION 3.25.1)
set(CMAKE_CXX_STANDARD 23)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT DEFINED ENV{RUNNING_ON_GITHUB})

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")

endif()

project(ng)

if(DEFINED ENV{RUNNING_ON_GITHUB})
set(SDL_UNIX_CONSOLE_BUILD ON)
endif()

set(SDL_STATIC ON CACHE BOOL "Build SDL as a static library." FORCE)

add_subdirectory(vendored/Catch2@v3.8.1 EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL@release-3.2.22 EXCLUDE_FROM_ALL)
add_subdirectory(vendored/imgui)

Include(CTest)
Include(Catch)

set(NG_LIB_SRC
        src/ast/ast.cpp
        src/token.cpp
        src/parsing/LexState.cpp
        src/parsing/Lexer.cpp
        src/parsing/ParseState.cpp
        src/parsing/ParserImpl.cpp
        src/ast/AstVisitor.cpp
        src/ast/DummyVisitor.cpp
        src/intp/stupid.cpp
        src/runtime/NGBoolean.cpp
        src/runtime/runtime_env.cpp
        src/runtime/NGString.cpp
        src/runtime/NGArray.cpp
        src/runtime/NGNumeral.cpp
        src/runtime/NGStructuralObject.cpp
        src/runtime/NGModule.cpp
        src/runtime/NGTuple.cpp
        src/runtime/NGRange.cpp
        src/runtime/NGUnit.cpp
        src/runtime/NGTaggedValue.cpp
        src/runtime/NGReference.cpp
        src/runtime/buffer_runtime.cpp
        src/runtime/storage_layout.cpp
        src/runtime/managed_heap.cpp
        src/sysdep/process.cpp
        src/stdlib/prelude.cpp
        src/stdlib/imgui.cpp
        src/module/ModuleLoader.cpp
        src/module/ModuleRegistry.cpp
        src/typecheck/typecheck.cpp
        src/typecheck/typecheck_utils.cpp
        src/typecheck/mangling.cpp
        src/typecheck/typeinfo.cpp
        src/typecheck/pattern_matching.cpp
        src/typecheck/overload_resolver.cpp
        src/typecheck/trait_resolution.cpp
        src/typecheck/receiver_effect_collector.cpp
        src/typecheck/PrimitiveType.cpp
        src/typecheck/FunctionType.cpp
        src/typecheck/ArrayType.cpp
        src/typecheck/TupleType.cpp
        src/typecheck/GenericType.cpp
        src/orgasm/Compiler.cpp
        src/orgasm/VM.cpp
        src/orgasm/module.cpp
        )

set(NG_TEST_SRC
        test/parsing/lexer_test.cpp
        test/parsing/lexer_keyword_test.cpp
        test/parsing/lexer_symbol_identifier_test.cpp
        test/parsing/lexer_values_test.cpp
        test/parsing/lexer_tuple_range_test.cpp
        test/parsing/parser_test.cpp
        test/parsing/parser_function_test.cpp
        test/parsing/parser_module_import_export_test.cpp
        test/parsing/parser_type_object_test.cpp
        test/parsing/parser_control_flow_test.cpp
        test/parsing/parser_unary_expression_test.cpp
        test/parsing/parse_composite_type_test.cpp
        test/parsing/parse_tuple_range_test.cpp
        test/parsing/parser_ref_move_test.cpp
        test/parsing/parser_generics_test.cpp
        test/parsing/parser_traits_test.cpp
        test/vnext/syntax_parser_test.cpp
        test/vnext/postfix_expression_test.cpp
        test/vnext/block_parser_test.cpp
        test/vnext/module_parser_test.cpp
        test/vnext/type_parser_test.cpp
        test/vnext/hir_resolver_test.cpp
        test/vnext/flowir_test.cpp
        test/vnext/bytecode_test.cpp
        test/vnext/vm_test.cpp
        test/vnext/typecheck_next_test.cpp
        test/vnext/const_expr_parser_test.cpp
        test/vnext/const_eval_test.cpp
        test/vnext/const_typecheck_test.cpp
        test/vnext/const_generic_test.cpp
        test/vnext/const_generic_call_test.cpp
        test/vnext/const_if_test.cpp
        test/vnext/const_declaration_test.cpp
        test/vnext/const_fun_test.cpp
        test/vnext/where_clause_test.cpp
        test/vnext/trait_stage1_test.cpp
        test/vnext/module_import_test.cpp
        test/vnext/move_semantics_test.cpp
        test/vnext/native_function_test.cpp
        test/vnext/stdlib_test.cpp
        test/vnext/variadic_pack_test.cpp
        test/vnext/drop_test.cpp
        test/vnext/range_slice_test.cpp
        test/vnext/borrow_check_test.cpp
        test/vnext/map_comprehension_test.cpp
        test/vnext/ref_type_test.cpp
        test/vnext/ref_runtime_test.cpp
        test/vnext/switch_statement_test.cpp
        test/vnext/driver_test.cpp
        test/vnext/value_test.cpp
        test/module/native_module_artifact_test.cpp
        test/integration_test.cpp
        test/runtime/interpreter_test.cpp
        test/runtime/nominal_test.cpp
        test/runtime/runtime_numeral_test.cpp
        test/runtime/tagged_value_test.cpp
        test/runtime/runtime_value_ops_test.cpp
        test/runtime/buffer_runtime_test.cpp
        test/typecheck/typeinfo_primitive_test.cpp
        test/typecheck/typeinfo_function_test.cpp
        test/typecheck/typeinfo_generic_test.cpp
        test/typecheck/typecheck_primitive_test.cpp
        test/typecheck/typecheck_function_test.cpp
        test/typecheck/typecheck_expression_test.cpp
        test/typecheck/typecheck_array_test.cpp
        test/typecheck/typecheck_ref_move_test.cpp
        test/typecheck/typecheck_generic_test.cpp
        test/typecheck/typecheck_traits_test.cpp
        test/typecheck/typecheck_parameter_pack_test.cpp
        test/typecheck/typecheck_controlflow_test.cpp
        test/typecheck/typecheck_tuple_test.cpp
        test/typecheck/typecheck_union_test.cpp
        test/typecheck/nominal_test.cpp
        test/orgasm/compiler_vm_test.cpp
        test/orgasm/examples_test.cpp
        test/orgasm/module_test.cpp
        test/orgasm/native_bridge_test.cpp
        test/orgasm/nominal_test.cpp
)

add_executable(feature_test test/feature_test.cpp)

add_library(ng ${NG_LIB_SRC})
target_include_directories(ng PUBLIC include)
target_link_libraries(ng PRIVATE imgui SDL3::SDL3-static)

set(NG_VNEXT_SRC
        src/vnext/driver.cpp
        src/vnext/bytecode.cpp
        src/vnext/bytecode/artifact.cpp
        src/vnext/flowir.cpp
        src/vnext/hir.cpp
        src/vnext/typecheck.cpp
        src/vnext/type_interner.cpp
        src/vnext/const_eval.cpp
        src/vnext/const_interp.cpp
        src/vnext/module_loader.cpp
        src/vnext/vm.cpp
        src/vnext/vm/value_ops.cpp
        src/vnext/syntax/block_parser.cpp
        src/vnext/syntax/const_expr.cpp
        src/vnext/syntax/module_parser.cpp
        src/vnext/syntax/parser.cpp
        src/vnext/syntax/type_parser.cpp
)

add_library(ng_vnext ${NG_VNEXT_SRC})
target_include_directories(ng_vnext PUBLIC include)

# region ngi

# `ngi` is the replacement frontend. It deliberately has no link dependency on
# the legacy interpreter/runtime library.
add_executable(ngi src/vnext/main.cpp)
target_link_libraries(ngi PRIVATE ng_vnext)

if (APPLE)
target_compile_options(ng PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping -coverage)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
target_compile_options(ng PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()
# endregion ngi


# region ng_test

add_executable(ng_test ${NG_TEST_SRC})
target_link_libraries(ng_test PRIVATE ng ng_vnext Catch2::Catch2WithMain)

if (APPLE)
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping -coverage)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping)
endif()
# endregion ng_test

# region ng_imgui_demo

add_executable(ng_imgui_demo test/imgui/main.cpp)
target_link_libraries(ng_imgui_demo PRIVATE imgui SDL3::SDL3-static)

# endregion ng_imgui_demo

catch_discover_tests(ng_test)
