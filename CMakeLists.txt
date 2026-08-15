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

add_subdirectory(vendored/Catch2@v3.8.1 EXCLUDE_FROM_ALL)

Include(CTest)
Include(Catch)

set(NG_SRC
        src/driver.cpp
        src/bytecode.cpp
        src/bytecode/artifact.cpp
        src/flowir.cpp
        src/hir.cpp
        src/typecheck.cpp
        src/type_interner.cpp
        src/const_eval.cpp
        src/const_interp.cpp
        src/module_loader.cpp
        src/vm.cpp
        src/vm/value_ops.cpp
        src/syntax/block_parser.cpp
        src/syntax/const_expr.cpp
        src/syntax/module_parser.cpp
        src/syntax/parser.cpp
        src/syntax/type_parser.cpp
)

add_library(ng ${NG_SRC})
target_include_directories(ng PUBLIC include)

# region ngi

# `ngi` is the NG frontend over the promoted vNext pipeline.
add_executable(ngi src/main.cpp)
target_link_libraries(ngi PRIVATE ng)

if (APPLE)
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping -coverage)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()
# endregion ngi


# region ng_test

file(GLOB NG_TEST_SRC CONFIGURE_DEPENDS test/*.cpp)
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

catch_discover_tests(ng_test)
