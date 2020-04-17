# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_LOOP "Enable loop library" TRUE)
if(NOT GG_LIBS_ENABLE_LOOP)
    return()
endif()

set(SOURCES gg_loop.c gg_loop_base.c)
set(HEADERS gg_loop.h gg_loop_base.h)

add_library(gg-loop ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-loop)

target_link_libraries(gg-loop PRIVATE gg-common)

include(ports/bsd/CMakeLists.txt)
include(ports/generic/CMakeLists.txt)

set_target_properties(gg-loop PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-loop EXPORT golden-gate
                        ARCHIVE DESTINATION lib
                        PUBLIC_HEADER DESTINATION include/xp/loop)
