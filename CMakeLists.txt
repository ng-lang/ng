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
)

FetchContent_MakeAvailable(Catch2)

Include(Catch)

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
        src/module/ModuleLoader.cpp)

set(NG_TEST_SRC
        test/parsing/lexer_test.cpp
        test/parsing/parser_test.cpp
        test/integration_test.cpp
        test/runtime/interpreter_test.cpp
        )

include_directories(include)

add_library(ng ${NG_LIB_SRC})

add_executable(ngi src/main.cpp)
target_link_libraries(ngi ng)

if (APPLE)
target_compile_options(ng PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ngi PRIVATE -coverage)
endif()

add_executable(ng_test ${NG_TEST_SRC})
target_link_libraries(ng_test PRIVATE ng Catch2::Catch2WithMain)

if (APPLE)
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE -coverage)
endif()
