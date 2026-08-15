# AI-generated code; reviewed for this repository's vNext rewrite.
# Integration of the vendored QBE backend (vendored/qbe-1.3) into the NG build.
#
# QBE is a standalone C99 program and upstream ships only a POSIX Makefile.
# This script builds the same `qbe` executable with CMake so the native code
# generator can find it next to `ngi` and drive it as a subprocess
# (QBE IL -> assembly; assembly/link are handed to the system `cc` later).
#
# Upstream Makefile parity:
#   - identical source list: all three targets (amd64, arm64, rv64) are
#     compiled into one binary; the target is selected at runtime with
#     `-t <target>` and defaults to the host via `Deftgt`;
#   - `config.h` is generated into the build tree exactly like `make config.h`;
#   - identical warning flags (-Wall -Wextra -Wpedantic, no -Werror).
#
# Sets NG_QBE_AVAILABLE to TRUE/FALSE for the including scope.

set(NG_QBE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendored/qbe-1.3")

if(NOT EXISTS "${NG_QBE_SRC_DIR}/main.c")
  message(STATUS "QBE is not vendored at ${NG_QBE_SRC_DIR}; skipping the `qbe` tool target")
  set(NG_QBE_AVAILABLE FALSE)
  return()
endif()

set(QBE_COMMON_SOURCES
  ${NG_QBE_SRC_DIR}/main.c ${NG_QBE_SRC_DIR}/util.c ${NG_QBE_SRC_DIR}/parse.c
  ${NG_QBE_SRC_DIR}/abi.c ${NG_QBE_SRC_DIR}/cfg.c ${NG_QBE_SRC_DIR}/mem.c
  ${NG_QBE_SRC_DIR}/ssa.c ${NG_QBE_SRC_DIR}/alias.c ${NG_QBE_SRC_DIR}/load.c
  ${NG_QBE_SRC_DIR}/copy.c ${NG_QBE_SRC_DIR}/fold.c ${NG_QBE_SRC_DIR}/gvn.c
  ${NG_QBE_SRC_DIR}/gcm.c ${NG_QBE_SRC_DIR}/simpl.c ${NG_QBE_SRC_DIR}/ifopt.c
  ${NG_QBE_SRC_DIR}/live.c ${NG_QBE_SRC_DIR}/spill.c ${NG_QBE_SRC_DIR}/rega.c
  ${NG_QBE_SRC_DIR}/emit.c)
set(QBE_AMD64_SOURCES
  ${NG_QBE_SRC_DIR}/amd64/targ.c ${NG_QBE_SRC_DIR}/amd64/sysv.c
  ${NG_QBE_SRC_DIR}/amd64/isel.c ${NG_QBE_SRC_DIR}/amd64/emit.c
  ${NG_QBE_SRC_DIR}/amd64/winabi.c)
set(QBE_ARM64_SOURCES
  ${NG_QBE_SRC_DIR}/arm64/targ.c ${NG_QBE_SRC_DIR}/arm64/abi.c
  ${NG_QBE_SRC_DIR}/arm64/isel.c ${NG_QBE_SRC_DIR}/arm64/emit.c)
set(QBE_RV64_SOURCES
  ${NG_QBE_SRC_DIR}/rv64/targ.c ${NG_QBE_SRC_DIR}/rv64/abi.c
  ${NG_QBE_SRC_DIR}/rv64/isel.c ${NG_QBE_SRC_DIR}/rv64/emit.c)

add_executable(qbe
  ${QBE_COMMON_SOURCES} ${QBE_AMD64_SOURCES} ${QBE_ARM64_SOURCES} ${QBE_RV64_SOURCES})

target_include_directories(qbe PRIVATE "${NG_QBE_SRC_DIR}")
set_target_properties(qbe PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  # Place the binary at the build root, next to `ngi`, so the native backend
  # finds it as a sibling tool without installation.
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_compile_options(qbe PRIVATE -Wall -Wextra -Wpedantic)

# Equivalent of `make config.h`: pick the default target for this host.
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(QBE_DEFTGT "T_arm64_apple")
elseif(APPLE)
  set(QBE_DEFTGT "T_amd64_apple")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(QBE_DEFTGT "T_arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
  set(QBE_DEFTGT "T_rv64")
else()
  set(QBE_DEFTGT "T_amd64_sysv")
endif()

set(QBE_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/qbe_config")
file(MAKE_DIRECTORY "${QBE_CONFIG_DIR}")
file(WRITE "${QBE_CONFIG_DIR}/config.h" "#define Deftgt ${QBE_DEFTGT}\n")
target_include_directories(qbe PRIVATE "${QBE_CONFIG_DIR}")

set(NG_QBE_AVAILABLE TRUE)

# End-to-end toolchain smoke test: QBE IL -> assembly (qbe) -> executable
# (system cc) -> run and verify. This exercises the exact pipeline the native
# backend will use. Registered only on Unix hosts with a `cc` toolchain.
if(UNIX)
  find_program(NG_CC_TOOL cc)
  if(NG_CC_TOOL)
    add_test(NAME qbe_smoke
      COMMAND ${CMAKE_COMMAND}
        "-DQBE_TOOL=$<TARGET_FILE:qbe>"
        "-DSSA_FILE=${CMAKE_CURRENT_SOURCE_DIR}/test/native/qbe_smoke.ssa"
        "-DCC_TOOL=${NG_CC_TOOL}"
        "-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/qbe_smoke"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/qbe_smoke.cmake")
  endif()
endif()
