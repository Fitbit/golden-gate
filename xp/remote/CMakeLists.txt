# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_REMOTE "Enable Remote API library" TRUE)
if(NOT GG_LIBS_ENABLE_REMOTE)
    return()
endif()

set(SOURCES gg_remote.c)
set(HEADERS gg_remote.h)

add_library(gg-remote ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-remote)

target_link_libraries(gg-remote PUBLIC gg-common
                                       gg-smo)

include(transport/serial/CMakeLists.txt)

set_target_properties(gg-remote PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-remote EXPORT golden-gate
                          ARCHIVE DESTINATION lib
                          PUBLIC_HEADER DESTINATION include/xp/remote)
