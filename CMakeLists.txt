cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

project(ng)

set(NG_LIB_SRC
        src/ast/ast.cpp
        src/ast/pprint.cpp
        src/ast/serializer.cpp
        src/base.cpp
        src/lexer.cpp
        src/parser.cpp
        src/visitor.cpp
        src/interpreter.cpp
        )

set(NG_TEST_SRC
        test/parsing/lexer_test.cpp
        test/parsing/parser_test.cpp
        test/integration_test.cpp
        test/runtime/interpreter_test.cpp
        test/test_runner.cpp
        )
include_directories(include)

add_library(ng SHARED ${NG_LIB_SRC})
add_executable(ngc src/main.cpp)
target_link_libraries(ngc ng)

add_executable(ngtest ${NG_TEST_SRC})
target_link_libraries(ngtest ng)

enable_testing()

add_test(NAME TEST COMMAND ngtest)
