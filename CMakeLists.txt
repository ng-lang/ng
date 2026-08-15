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
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping -coverage)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
target_link_options(ngi PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()
# endregion ngi


# region ng_test

file(GLOB NG_TEST_SRC CONFIGURE_DEPENDS test/vnext/*.cpp)
add_executable(ng_test ${NG_TEST_SRC})
target_link_libraries(ng_test PRIVATE ng_vnext Catch2::Catch2WithMain)

if (APPLE)
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping -coverage)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping)
endif()
# endregion ng_test

catch_discover_tests(ng_test)
