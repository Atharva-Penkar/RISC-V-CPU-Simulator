cmake_minimum_required(VERSION 3.20)
project(riscv_sim VERSION 1.0.0 DESCRIPTION "RV32I assembler and 5-stage pipelined CPU simulator" LANGUAGES CXX)

option(RV_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(RV_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler options shared by every target.
add_library(rv_options INTERFACE)
if(MSVC)
  target_compile_options(rv_options INTERFACE /W4 /permissive- /utf-8)
  if(RV_WARNINGS_AS_ERRORS)
    target_compile_options(rv_options INTERFACE /WX)
  endif()
else()
  target_compile_options(rv_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  if(RV_WARNINGS_AS_ERRORS)
    target_compile_options(rv_options INTERFACE -Werror)
  endif()
  if(RV_SANITIZE)
    target_compile_options(rv_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(rv_options INTERFACE -fsanitize=address,undefined)
  endif()
endif()

add_library(rv
  src/asm/lexer.cpp
  src/asm/parser.cpp
  src/asm/encoder.cpp
  src/sim/decoder.cpp
  src/sim/alu.cpp
  src/sim/memory.cpp
  src/sim/iss.cpp
  src/sim/pipeline.cpp
  src/sim/hazard.cpp
)
target_include_directories(rv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(rv PRIVATE rv_options)

add_executable(rvasm apps/rvasm.cpp)
target_link_libraries(rvasm PRIVATE rv rv_options)

add_executable(rvsim apps/rvsim.cpp)
target_link_libraries(rvsim PRIVATE rv rv_options)

include(CTest)
if(BUILD_TESTING)
  add_executable(unit_tests
    tests/unit/main.cpp
    tests/unit/test_isa.cpp
    tests/unit/test_assembler.cpp
    tests/unit/test_alu.cpp
    tests/unit/test_memory.cpp
    tests/unit/test_cpu.cpp
  )
  target_link_libraries(unit_tests PRIVATE rv rv_options)
  add_test(NAME unit COMMAND unit_tests)

  # Every program runs on both CPU models and must produce the expected final state.
  file(GLOB RV_TEST_PROGRAMS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/*.s)
  foreach(program ${RV_TEST_PROGRAMS})
    get_filename_component(name ${program} NAME_WE)
    foreach(model pipeline iss)
      add_test(NAME program.${name}.${model}
        COMMAND ${CMAKE_COMMAND}
          -DRVSIM=$<TARGET_FILE:rvsim> -DMODEL=${model} -DPROGRAM=${program}
          -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/programs/${name}.expected
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_program.cmake)
    endforeach()
    add_test(NAME assemble.${name} COMMAND rvasm ${program})
  endforeach()
endif()
