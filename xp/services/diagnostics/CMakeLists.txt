# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_RAM_STORAGE_SERVICE "Enable Diagnostics RAM Storage service" FALSE)
if(NOT GG_LIBS_ENABLE_RAM_STORAGE_SERVICE)
    return()
endif()

set(SOURCES gg_ram_storage_service.c)
set(HEADERS gg_ram_storage_service.h)

add_library(gg-ram-storage-service ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-ram-storage-service)

target_link_libraries(gg-ram-storage-service PRIVATE gg-common
                                                     gg-diagnostics
                                             PUBLIC gg-remote)

set_target_properties(gg-ram-storage-service PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-ram-storage-service EXPORT golden-gate
                                       ARCHIVE DESTINATION lib
                                       PUBLIC_HEADER DESTINATION include/xp/services/diagnostics)
