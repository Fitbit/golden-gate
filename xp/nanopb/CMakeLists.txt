# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_PORTS_ENABLE_NANOPB "Enable nanopb port" FALSE)
if(NOT GG_PORTS_ENABLE_NANOPB)
    return()
endif()

# Root of the Nanopb distribution
set(NANOPB_ROOT ${GG_ROOT}/external/nanopb)

add_library(gg-nanopb ${NANOPB_ROOT}/pb.h
                      ${NANOPB_ROOT}/pb_common.c
                      ${NANOPB_ROOT}/pb_common.h
                      ${NANOPB_ROOT}/pb_decode.c
                      ${NANOPB_ROOT}/pb_decode.h
                      ${NANOPB_ROOT}/pb_encode.c
                      ${NANOPB_ROOT}/pb_encode.h)

target_include_directories(gg-nanopb PUBLIC ${NANOPB_ROOT})

# disable some warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(gg-nanopb PRIVATE -Wno-covered-switch-default -Wno-switch-enum)
endif()
