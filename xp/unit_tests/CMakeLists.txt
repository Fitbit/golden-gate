# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

project (gg-unit-tests VERSION 0.1 LANGUAGES C)

# Our unit tests are written in CPP
enable_language(CXX)

# CppUTest Setup
###################

# Get the include path and library path from conda
if(NOT DEFINED ENV{CONDA_PREFIX})
    message(FATAL_ERROR "CONDA_PREFIX is not set, but it is required to get CppUTest headers and libraries")
endif()

set(CPPUTEST_LIB_DIR $ENV{CONDA_PREFIX}/lib)
set(CPPUTEST_INC_DIR $ENV{CONDA_PREFIX}/include)
find_library(CPPUTEST_LIB_FILE CppUTest PATHS ${CPPUTEST_LIB_DIR} REQUIRED NO_DEFAULT_PATH)
find_library(CPPUTEST_EXT_LIB_FILE CppUTestExt PATHS ${CPPUTEST_LIB_DIR} REQUIRED NO_DEFAULT_PATH)
set(CPPUTEST_LIBS ${CPPUTEST_LIB_FILE} ${CPPUTEST_EXT_LIB_FILE})

# Define a function to allow the subdirectories to easily define tests
function(gg_add_test TEST_SOURCE PRODUCT_LIBS)
  get_filename_component(TEST_EXE_BASENAME ${TEST_SOURCE} NAME_WE)
  string(CONCAT TEST_EXE_NAME ${TEST_EXE_BASENAME} "_runme")

  add_executable(${TEST_EXE_NAME} ../test_runner.cpp ${TEST_SOURCE})
  target_include_directories(${TEST_EXE_NAME} PRIVATE ${CPPUTEST_INC_DIR})
  target_link_libraries(${TEST_EXE_NAME} PRIVATE ${CPPUTEST_LIBS} ${PRODUCT_LIBS})

    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${TEST_EXE_NAME} PRIVATE -Weverything)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-unused-but-set-variable -Wno-padded -Wno-reserved-id-macro)
        target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-weak-vtables -Wno-global-constructors -Wno-exit-time-destructors)
        target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-disabled-macro-expansion -Wno-old-style-cast -Wno-keyword-macro)
        target_compile_options(${TEST_EXE_NAME} PRIVATE -Wno-c++11-long-long -Wno-missing-field-initializers -Wno-variadic-macros)
    endif()

  set(TEST_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${TEST_EXE_NAME})
  add_test(NAME ${TEST_EXE_NAME} COMMAND ${TEST_COMMAND})
endfunction(gg_add_test)

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
