# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_DIAGNOSTICS "Enable Device Diagnostics library" TRUE)
if(NOT GG_LIBS_ENABLE_DIAGNOSTICS)
    return()
endif()

set(SOURCES gg_diagnostics_ram_storage.c)
set(HEADERS gg_diagnostics_ram_storage.h)

add_library(gg-diagnostics ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-diagnostics)

target_link_libraries(gg-diagnostics PUBLIC gg-common)

set_target_properties(gg-diagnostics PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-diagnostics EXPORT golden-gate
                               ARCHIVE DESTINATION lib
                               PUBLIC_HEADER DESTINATION include/xp/diagnostics)
