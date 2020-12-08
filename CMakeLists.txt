cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)

project(ng)

find_package(Canvas REQUIRED)

find_package(Catch2 REQUIRED)

set(NG_LIB_SRC
        src/ast/ast.cpp
        src/ast/serializer.cpp
        src/base.cpp
        src/lexer.cpp
        src/parser.cpp
        src/visitor.cpp
        src/intp/interpreter.cpp
        src/intp/runtime.cpp
        )

set(NG_TEST_SRC
        test/parsing/lexer_test.cpp
        test/parsing/parser_test.cpp
        test/integration_test.cpp
        test/runtime/interpreter_test.cpp
        )

include_directories(include)

add_library(ng SHARED ${NG_LIB_SRC})

target_link_libraries(ng canvas)

add_executable(ngi src/main.cpp)
target_link_libraries(ngi ng)

add_executable(ng_test ${NG_TEST_SRC})
target_link_libraries(ng_test ng catch2)
