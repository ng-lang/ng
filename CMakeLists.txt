cmake_minimum_required(VERSION 4.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_BUILD_TYPE Debug)

project(ng)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)

set(NG_LIB_SRC
        src/ast/ast.cpp
        src/NonCopyable.cpp
        src/parsing/LexState.cpp
        src/parsing/Lexer.cpp
        src/parsing/ParseState.cpp
        src/parsing/ParserImpl.cpp
        src/ast/IASTVisitor.cpp
        src/ast/DefaultDummyAstVisitor.cpp
        src/intp/stupid.cpp
        src/runtime/IBasicObject.cpp
        src/runtime/IOverloadableOperators.cpp
        src/runtime/NGObject.cpp
        src/runtime/NGBoolean.cpp
        src/runtime/NGInteger.cpp
        src/runtime/NGString.cpp
        src/runtime/NGArray.cpp
        src/runtime/NGStructuralObject.cpp
        src/runtime/NGModule.cpp
        src/module/IModuleLoader.cpp)

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

add_executable(ng_test ${NG_TEST_SRC})
target_link_libraries(ng_test PRIVATE ng Catch2::Catch2WithMain)
