# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

project (gg-unit-tests VERSION 0.1 LANGUAGES C)

# Our unit tests are written in CPP
enable_language(CXX)

##########################################################################
# Add cpputest directly from the submodule.
# This defines the CppUTest and CppUTestExt targets.
#
set(CPPUTEST_ROOT ${GG_ROOT}/external/cpputest)

# Don't build CppUTest's own tests.
# Unfortunately, CppUTest's CMake uses options withou a module prefix...
option(TESTS "" OFF)

# CPPuTest's CMake files force-set CMAKE_BUILD_TYPE when not set, so we're
# better off setting it here first
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

# Now configure CPPuTest
add_subdirectory(${CPPUTEST_ROOT}
                 ${CMAKE_CURRENT_BINARY_DIR}/cpputest-build
                 EXCLUDE_FROM_ALL)

# Adjust some compiler options
if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(CppUTest PRIVATE -Wno-cast-qual -Wno-comma)
    target_compile_options(CppUTestExt PRIVATE -Wno-cast-qual -Wno-comma)
endif()
##########################################################################

# Define a function to allow the subdirectories to easily define tests
function(gg_add_test TEST_SOURCE PRODUCT_LIBS)
  get_filename_component(TEST_EXE_BASENAME ${TEST_SOURCE} NAME_WE)
  string(CONCAT TEST_EXE_NAME ${TEST_EXE_BASENAME} "_runme")

  add_executable(${TEST_EXE_NAME} ../test_runner.cpp ${TEST_SOURCE})
  target_link_libraries(${TEST_EXE_NAME} PRIVATE CppUTest CppUTestExt ${PRODUCT_LIBS})
  target_include_directories(${TEST_EXE_NAME} PRIVATE ${CPPUTEST_ROOT}/include)

  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(${TEST_EXE_NAME} PRIVATE -Weverything)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-cast-qual -Wno-unused-parameter -Wno-unused-variable)
    target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-unused-but-set-variable -Wno-padded -Wno-reserved-id-macro)
    target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-weak-vtables -Wno-global-constructors -Wno-exit-time-destructors)
    target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-disabled-macro-expansion -Wno-old-style-cast -Wno-keyword-macro)
    target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-c++11-long-long -Wno-missing-field-initializers -Wno-variadic-macros)
  endif()

  set(TEST_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${TEST_EXE_NAME})
  add_test(NAME ${TEST_EXE_NAME} COMMAND ${TEST_COMMAND})
endfunction(gg_add_test)

# Add all test subdirectories
add_subdirectory(common)
add_subdirectory(diagnostics)
add_subdirectory(gattlink)
add_subdirectory(remote)
add_subdirectory(lwip)
add_subdirectory(coap)
add_subdirectory(protocols)
add_subdirectory(loop)
add_subdirectory(dtls)
add_subdirectory(module)
add_subdirectory(utils)
add_subdirectory(nip)
add_subdirectory(sockets)
add_subdirectory(services)
add_subdirectory(stack_builder)
