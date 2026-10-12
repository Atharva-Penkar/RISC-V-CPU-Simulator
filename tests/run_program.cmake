# Runs one test program on rvsim and compares the final state dump with the expected file.
# Usage: cmake -DRVSIM=<rvsim> -DMODEL=<pipeline|iss> -DPROGRAM=<file.s> -DEXPECTED=<file.expected> -P run_program.cmake
execute_process(
  COMMAND ${RVSIM} --model ${MODEL} --no-stats ${PROGRAM}
  OUTPUT_VARIABLE actual
  ERROR_VARIABLE log
  RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "rvsim exited with status ${status}\n${log}")
endif()

file(READ ${EXPECTED} expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REPLACE "\r\n" "\n" actual "${actual}")
if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "final state differs from ${EXPECTED}\n--- expected\n${expected}--- actual\n${actual}")
endif()
