cmake_minimum_required(VERSION 3.25.1)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_BUILD_TYPE Debug)

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
        src/runtime/ObjectBase.cpp
        src/runtime/OperatorsBase.cpp
        src/runtime/NGObject.cpp
        src/runtime/NGBoolean.cpp
        src/runtime/NGContext.cpp
        src/runtime/NGString.cpp
        src/runtime/NGArray.cpp
        src/runtime/NGNumeral.cpp
        src/runtime/NGStructuralObject.cpp
        src/runtime/NGModule.cpp
        src/runtime/NGTuple.cpp
        src/runtime/NGUnit.cpp
        src/sysdep/process.cpp
        src/stdlib/prelude.cpp
        src/stdlib/imgui.cpp
        src/module/ModuleLoader.cpp
        src/module/ModuleRegistry.cpp
        src/typecheck/typecheck.cpp
        src/typecheck/typeinfo.cpp
        src/typecheck/PrimitiveType.cpp
        src/typecheck/FunctionType.cpp
        src/typecheck/ArrayType.cpp
        src/typecheck/TupleType.cpp
        src/orgasm/lexer.cpp
        src/orgasm/parser.cpp
        src/orgasm/interpreter.cpp
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
        test/integration_test.cpp
        test/runtime/interpreter_test.cpp
        test/runtime/runtime_numeral_test.cpp
        test/typecheck/typeinfo_primitive_test.cpp
        test/typecheck/typeinfo_function_test.cpp
        test/typecheck/typecheck_primitive_test.cpp
        test/typecheck/typecheck_function_test.cpp
        test/typecheck/typecheck_expression_test.cpp
        test/typecheck/typecheck_array_test.cpp
        test/typecheck/typecheck_controlflow_test.cpp
        test/typecheck/typecheck_tuple_test.cpp
        test/orgasm/lexer_test.cpp
        test/orgasm/parser_test.cpp
        test/orgasm/interpreter_test.cpp
)

add_executable(feature_test test/feature_test.cpp)

add_library(ng ${NG_LIB_SRC})
target_include_directories(ng PUBLIC include)
target_link_libraries(ng PRIVATE imgui SDL3::SDL3-static)

# region ngi

add_executable(ngi src/main.cpp)
target_link_libraries(ngi PRIVATE ng)

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
target_link_libraries(ng_test PRIVATE ng Catch2::Catch2WithMain)

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