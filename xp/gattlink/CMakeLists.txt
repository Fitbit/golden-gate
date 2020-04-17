# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_GATTLINK "Enable gattlink library" TRUE)
if(NOT GG_LIBS_ENABLE_GATTLINK)
    return()
endif()

set(SOURCES gg_gattlink.c gg_gattlink_generic_client.c)
set(HEADERS gg_gattlink.h gg_gattlink_generic_client.h)

add_library(gg-gattlink ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-gattlink)

target_link_libraries(gg-gattlink PRIVATE gg-annotations gg-common gg-protocols gg-utils)

set_target_properties(gg-gattlink PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-gattlink EXPORT golden-gate
                        ARCHIVE DESTINATION lib
                        PUBLIC_HEADER DESTINATION include/xp/gattlink)
