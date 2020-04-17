# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_BLAST_SERVICE "Enable blast service" TRUE)
if(NOT GG_LIBS_ENABLE_BLAST_SERVICE)
    return()
endif()

set(SOURCES gg_blast_service.c)
set(HEADERS gg_blast_service.h)

add_library(gg-blast-service ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-blast-service)

target_link_libraries(gg-blast-service PRIVATE gg-common
                                               gg-loop
                                               gg-utils
                                       PUBLIC gg-remote)

set_target_properties(gg-blast-service PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-blast-service EXPORT golden-gate
                                 ARCHIVE DESTINATION lib
                                 PUBLIC_HEADER DESTINATION include/xp/services/blast)
