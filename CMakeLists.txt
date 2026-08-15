cmake_minimum_required(VERSION 3.25.1)
set(CMAKE_CXX_STANDARD 23)
# clang-tidy/clang-format support: export the compilation database so
# `clang-tidy -p build` resolves include paths and flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT DEFINED ENV{RUNNING_ON_GITHUB})

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# clang-tidy/clang-format come from Homebrew LLVM (/opt/homebrew/opt/llvm).
# Emit an explicit -isysroot so clang-tidy resolves the SDK libc++ headers
# (its toolchain dir ships none) when reading build/compile_commands.json.
execute_process(COMMAND xcrun --sdk macosx --show-sdk-path OUTPUT_VARIABLE NG_MACOS_SDK OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isysroot ${NG_MACOS_SDK}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${NG_MACOS_SDK}")

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

# Vendored QBE backend tool (vendored/qbe-1.3): builds the `qbe` executable
# and registers the toolchain smoke test. The native code generator drives
# `qbe` as a subprocess (QBE IL -> assembly);
# see docs/design/rearchitecture/08-qbe-native-backend.md.
include(cmake/qbe.cmake)

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

if(NG_QBE_AVAILABLE)
  # The native backend shells out to the sibling `qbe` binary; expose its
  # absolute path as a compile definition and order the build after it.
  target_compile_definitions(ng PUBLIC "NG_QBE_PATH=\"$<TARGET_FILE:qbe>\"")
  add_dependencies(ng qbe)
endif()

# Instrument the pipeline sources so llvm-cov measures src/ and include/
# (the coverage gate ignores vendored/ and test/).
if (APPLE)
  target_compile_options(ng PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(ng PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()

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


# region ngi_imgui

# `ngi_imgui` is the same frontend plus the imgui native binding
# (`lib/std/imgui.ng`), powering the NG IDE example.
add_library(ng_imgui STATIC src/imgui_natives.cpp)
target_link_libraries(ng_imgui PUBLIC ng imgui)
target_include_directories(ng_imgui PUBLIC include)

add_executable(ngi_imgui src/imgui_main.cpp)
target_link_libraries(ngi_imgui PRIVATE ng_imgui)
if (APPLE)
target_link_options(ngi_imgui PRIVATE -fprofile-instr-generate -fcoverage-mapping -coverage)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
target_link_options(ngi_imgui PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()
# endregion ngi_imgui


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
