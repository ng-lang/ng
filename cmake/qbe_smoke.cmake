# AI-generated code; reviewed for this repository's vNext rewrite.
# CTest smoke script for the vendored QBE tool (see cmake/qbe.cmake).
# Pipeline under test: QBE IL -> assembly (qbe) -> executable (cc) -> run.
# Expects -DQBE_TOOL, -DSSA_FILE, -DCC_TOOL, -DOUT_DIR from add_test.

file(MAKE_DIRECTORY "${OUT_DIR}")

execute_process(
  COMMAND "${QBE_TOOL}" -o "${OUT_DIR}/smoke.s" "${SSA_FILE}"
  RESULT_VARIABLE qbe_result)
if(NOT qbe_result EQUAL 0)
  message(FATAL_ERROR "qbe failed to lower ${SSA_FILE}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(MATH_LIB "-lm")
else()
  set(MATH_LIB "")
endif()

execute_process(
  COMMAND "${CC_TOOL}" "${OUT_DIR}/smoke.s" -o "${OUT_DIR}/smoke" ${MATH_LIB}
  RESULT_VARIABLE cc_result)
if(NOT cc_result EQUAL 0)
  message(FATAL_ERROR "cc failed to assemble/link the qbe output")
endif()

execute_process(
  COMMAND "${OUT_DIR}/smoke"
  OUTPUT_VARIABLE smoke_output
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "smoke binary exited with ${run_result}")
endif()

if(NOT smoke_output STREQUAL "fib(10) = 55")
  message(FATAL_ERROR "smoke binary printed '${smoke_output}', expected 'fib(10) = 55'")
endif()

message(STATUS "qbe smoke: IL -> assembly -> executable -> ran, printed '${smoke_output}'")
