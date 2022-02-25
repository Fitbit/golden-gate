# Copyright 2021 Google LLC
# SPDX-License-Identifier: Apache-2.0

# Define a function to allow the subdirectories to easily define fuzz targets
function(gg_add_fuzzer FUZZER_SOURCE PRODUCT_LIBS)
  get_filename_component(FUZZER_EXE_BASENAME ${FUZZER_SOURCE} NAME_WE)
  string(CONCAT FUZZER_EXE_NAME ${FUZZER_EXE_BASENAME} "_runme")

  add_executable(${FUZZER_EXE_NAME} ${FUZZER_SOURCE})
  target_link_libraries(${FUZZER_EXE_NAME} PRIVATE ${PRODUCT_LIBS})

  if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
      target_compile_options(${FUZZER_EXE_NAME} PRIVATE -Wno-missing-prototypes)
  endif()

  endfunction(gg_add_fuzzer)

add_subdirectory(coap)
add_subdirectory(gattlink)
add_subdirectory(smo)
