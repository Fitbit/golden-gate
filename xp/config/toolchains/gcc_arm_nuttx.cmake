# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

if(DEFINED ENV{GG_NUTTX_ROOT})
  set(NUTTX_ROOT $ENV{GG_NUTTX_ROOT})
else()
    message(FATAL_ERROR "GG_NUTTX_ROOT environment variable is not defined")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/gcc_arm.cmake)
set(COMMON_FLAGS "-Os -fno-builtin -fdata-sections -ffunction-sections -fno-exceptions -fno-strict-aliasing -fno-strength-reduce -fomit-frame-pointer -ggdb -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -isystem ${NUTTX_ROOT}/include")

# need to put the CMAKE_<LANG>_FLAGS in cache since they will be read from there
unset(CMAKE_CXX_FLAGS CACHE)
unset(CMAKE_C_FLAGS CACHE)
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=gnu++0x" CACHE STRING "Flags used for compiling C++ files")
set(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=gnu99" CACHE STRING "Flags used for compiling C files")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-gc-sections ")
