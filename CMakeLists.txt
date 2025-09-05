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

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1 # or a later release
  GIT_PROGRESS   TRUE
)

FetchContent_Declare(
  SDL
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.2.22
  GIT_PROGRESS   TRUE
)

set(SDL_STATIC ON CACHE BOOL "Build SDL as a shared library." FORCE)

FetchContent_MakeAvailable(Catch2 SDL)

add_subdirectory(vendored/imgui)

set(NG_LIB_SRC
        src/ast/ast.cpp
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
        src/sysdep/process.cpp
        src/stdlib/prelude.cpp
        src/module/ModuleLoader.cpp
        src/module/ModuleRegistry.cpp)

set(NG_TEST_SRC
        test/parsing/lexer_test.cpp
        test/parsing/lexer_keyword_test.cpp
        test/parsing/lexer_symbol_identifier_test.cpp
        test/parsing/lexer_values_test.cpp
        test/parsing/parser_test.cpp
        test/parsing/parser_function_test.cpp
        test/parsing/parser_module_import_export_test.cpp
        test/parsing/parser_type_object_test.cpp
        test/integration_test.cpp
        test/runtime/interpreter_test.cpp
        )


add_library(ng ${NG_LIB_SRC})
target_include_directories(ng PUBLIC include)

add_executable(ngi src/main.cpp)
target_link_libraries(ngi PRIVATE ng imgui SDL3::SDL3-static)

add_executable(ng_imgui_demo test/imgui/main.cpp)
target_link_libraries(ng_imgui_demo PRIVATE imgui SDL3::SDL3-static)

if (APPLE)
target_compile_options(ng PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping -coverage)
endif()

if (DEFINED ENV{RUNNING_ON_GITHUB})
target_compile_options(ng PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()

add_executable(ng_test ${NG_TEST_SRC})
target_link_libraries(ng_test PRIVATE ng Catch2::Catch2WithMain)

if (APPLE)
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping -coverage)
endif()

if (DEFINED ENV{RUNNING_ON_GITHUB})
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping)
endif()

