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

# AddressSanitizer + UndefinedBehaviorSanitizer build variant (INC-0):
# configure with -DNG_ASAN=ON into a separate tree, e.g.
#   cmake -S . -B build-asan -GNinja -DNG_ASAN=ON && cmake --build build-asan -j
# Instruments the compiler pipeline (ng), the runtime archive (ngrt), and both
# frontends (ngi/ng_test) so memory errors surface inside the test process.
option(NG_ASAN "Instrument ng/ngrt/ngi/ng_test with -fsanitize=address,undefined." OFF)
if(NG_ASAN)
  set(NG_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

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
        src/native/const_hosts.cpp
        src/flowir.cpp
        src/flowir_expr.cpp
        src/flowir_loops.cpp
        src/flowir_switch.cpp
        src/native/lowering_aggregate.cpp
        src/native/lowering_call.cpp
        src/native/lowering_expr.cpp
        src/native/lowering_scaffold.cpp
        src/hir.cpp
        src/hir_body.cpp
        src/hir_clone.cpp
        src/hir_types.cpp
        src/typecheck.cpp
        src/typecheck_register.cpp
        src/typecheck_infer.cpp
        src/typecheck_expected.cpp
        src/typecheck_call.cpp
        src/typecheck_check.cpp
        src/typecheck_switch.cpp
        src/typecheck_ownership.cpp
        src/typecheck_const.cpp
        src/typecheck_unify.cpp
        src/typecheck_growth.cpp
        src/type_interner.cpp
        src/const_eval.cpp
        src/const_interp.cpp
        src/module_loader.cpp
        src/module_artifact.cpp
        src/native/lowering.cpp
        src/ngrt.cpp
        src/syntax/block_parser.cpp
        src/syntax/const_expr.cpp
        src/syntax/module_parser.cpp
        src/syntax/parser.cpp
        src/syntax/type_parser.cpp
)

add_library(ng ${NG_SRC})
target_include_directories(ng PUBLIC include)
if(NG_ASAN)
  # PUBLIC compile options propagate the instrumentation into ngi/ng_test;
  # INTERFACE link options make the executables pull in the sanitizer runtime.
  target_compile_options(ng PUBLIC ${NG_SANITIZER_FLAGS})
  target_link_options(ng INTERFACE ${NG_SANITIZER_FLAGS})
endif()

# libngrt: the single C implementation of the standard native functions.
# The native backend links this archive into every generated executable; the
# frontend also links it for the thin C++ wrappers used by const evaluation.
add_library(ngrt STATIC src/native/libngrt.c)
set_target_properties(ngrt PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
target_include_directories(ngrt PUBLIC include)
target_compile_options(ngrt PRIVATE -Wall -Wextra -Wpedantic)
# Note: ngrt deliberately stays uninstrumented under NG_ASAN — it is linked
# into generated executables assembled by the plain system cc, which would
# miss the sanitizer runtime (undefined ___asan_* symbols at link time).
# Memory-error guarding covers the compiler pipeline inside ngi/ng_test.

# ARCH-1 runtime helper archive: the QBE IL text under src/native/helpers is
# assembled ONCE at build time (qbe -> cc -> ar) instead of being hardcoded
# as C++ strings in lowering.cpp. Generated executables link the archive;
# the linker pulls only the members they reference, which replaces the old
# per-compilation helper emission bookkeeping.
set(NG_HELPER_SSA
        ${CMAKE_CURRENT_SOURCE_DIR}/src/native/helpers/checked.ssa
        ${CMAKE_CURRENT_SOURCE_DIR}/src/native/helpers/arr.ssa
        ${CMAKE_CURRENT_SOURCE_DIR}/src/native/helpers/str.ssa
        ${CMAKE_CURRENT_SOURCE_DIR}/src/native/helpers/ref.ssa
        ${CMAKE_CURRENT_SOURCE_DIR}/src/native/helpers/span.ssa)
set(NG_HELPERS_ARCHIVE ${CMAKE_BINARY_DIR}/libngrt_helpers.a)
if(NG_QBE_AVAILABLE)
  add_custom_command(
          OUTPUT ${NG_HELPERS_ARCHIVE}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/ng_helpers_obj
          COMMAND $<TARGET_FILE:qbe> -o ${CMAKE_CURRENT_BINARY_DIR}/ng_helpers_obj/helpers.s ${NG_HELPER_SSA}
          COMMAND cc -c -o ${CMAKE_CURRENT_BINARY_DIR}/ng_helpers_obj/helpers.o
                   ${CMAKE_CURRENT_BINARY_DIR}/ng_helpers_obj/helpers.s
          COMMAND ar rcs ${NG_HELPERS_ARCHIVE} ${CMAKE_CURRENT_BINARY_DIR}/ng_helpers_obj/helpers.o
          DEPENDS qbe ${NG_HELPER_SSA}
          COMMENT "Assembling libngrt_helpers.a from QBE IL"
          VERBATIM)
  add_custom_target(ngrt_helpers ALL DEPENDS ${NG_HELPERS_ARCHIVE})
  target_compile_definitions(ng PUBLIC "NG_NGRT_HELPERS_PATH=\"${NG_HELPERS_ARCHIVE}\"")
  add_dependencies(ng ngrt_helpers)
endif()
target_link_libraries(ng PUBLIC ngrt)
target_compile_definitions(ng PUBLIC "NG_NGRT_PATH=\"$<TARGET_FILE:ngrt>\"")
add_dependencies(ng ngrt)

if(NG_QBE_AVAILABLE)
  # The native backend shells out to the sibling `qbe` binary; expose its
  # absolute path as a compile definition and order the build after it.
  target_compile_definitions(ng PUBLIC "NG_QBE_PATH=\"$<TARGET_FILE:qbe>\"")
  add_dependencies(ng qbe)
endif()
# imgui AOT backend: C-ABI `ngrt_imgui*` symbols linked into generated
# executables when a program imports lib/std/imgui.ng.
add_library(ngrt_imgui STATIC src/native/imgui_ngrt.cpp)
target_link_libraries(ngrt_imgui PUBLIC imgui ngrt)
target_include_directories(ngrt_imgui PUBLIC include)
if(APPLE)
  set(NG_NGRT_IMGUI_SDL_LIBS "-framework CoreMedia -framework CoreVideo -framework Cocoa -weak_framework UniformTypeIdentifiers -framework IOKit -framework ForceFeedback -framework Carbon -framework CoreAudio -framework AudioToolbox -framework AVFoundation -framework Foundation -framework GameController -framework Metal -framework QuartzCore -weak_framework CoreHaptics")
else()
  set(NG_NGRT_IMGUI_SDL_LIBS "-ldl -lpthread")
endif()
target_compile_definitions(ng PUBLIC "NG_NGRT_IMGUI_PATH=\"$<TARGET_FILE:ngrt_imgui>\"")
# imgui/SDL3 archives are resolved at runtime by the driver (NG_LIBRARY_PATH /
# LIBRARY_PATH), with these build-tree paths as defaults; the value stays a
# space-separated argument list (whitespace-containing paths unsupported).
target_compile_definitions(ng PUBLIC "NG_IMGUI_PATH=\"$<TARGET_FILE:imgui>\"")
target_compile_definitions(ng PUBLIC "NG_SDL_PATH=\"$<TARGET_FILE:SDL3::SDL3-static>\"")
target_compile_definitions(ng PUBLIC "NG_NGRT_IMGUI_EXTRA=\"libimgui.a libSDL3.a ${NG_NGRT_IMGUI_SDL_LIBS}\"")
add_dependencies(ng ngrt_imgui)

if(NG_ASAN)
  # imgui glue and the vendored Dear ImGui archives are linked into generated
  # executables assembled by the plain system cc (no sanitizer runtime), the
  # same reason ngrt stays uninstrumented above — otherwise the final link
  # fails on unresolved ___asan_*/___ubsan_* symbols.
  target_compile_options(ngrt_imgui PRIVATE -fno-sanitize=address,undefined)
  target_compile_options(imgui PRIVATE -fno-sanitize=address,undefined)
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




# region ng_test

file(GLOB NG_TEST_SRC CONFIGURE_DEPENDS test/*.cpp)
add_executable(ng_test ${NG_TEST_SRC})
target_link_libraries(ng_test PRIVATE ng Catch2::Catch2WithMain)
target_compile_definitions(ng_test PRIVATE NG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

if (APPLE)
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping -coverage)
elseif (DEFINED ENV{RUNNING_ON_GITHUB} AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
target_compile_options(ng_test PRIVATE -fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)
target_link_options(ng_test PRIVATE  -fprofile-instr-generate -fcoverage-mapping)
endif()
# endregion ng_test

catch_discover_tests(ng_test)
