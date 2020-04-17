# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

if(MINGW OR CYGWIN OR WIN32)
    set(UTIL_SEARCH_CMD where)
elseif(UNIX OR APPLE)
    set(UTIL_SEARCH_CMD which)
endif()

set(TOOLCHAIN_PREFIX arm-none-eabi-)

execute_process(
  COMMAND ${UTIL_SEARCH_CMD} ${TOOLCHAIN_PREFIX}gcc
  OUTPUT_VARIABLE BINUTILS_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

get_filename_component(ARM_TOOLCHAIN_DIR ${BINUTILS_PATH} DIRECTORY)
# for automatic compilation tests
set(CMAKE_EXE_LINKER_FLAGS_INIT "--specs=nosys.specs")

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

set(CMAKE_OBJCOPY ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_SIZE_UTIL ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}size CACHE INTERNAL "size tool")

set(CMAKE_FIND_ROOT_PATH ${BINUTILS_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(COMMON_FLAGS "-mcpu=cortex-m4 -Os -mthumb -mthumb-interwork -msoft-float -ffunction-sections -fdata-sections -g -fno-common -fmessage-length=0")

# need to put the CMAKE_<LANG>_FLAGS in cache since they will be read from there
unset(CMAKE_CXX_FLAGS CACHE)
unset(CMAKE_C_FLAGS CACHE)
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=gnu++0x" CACHE STRING "Flags used for compiling C++ files")
set(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=gnu99" CACHE STRING "Flags used for compiling C files")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,-gc-sections ")
