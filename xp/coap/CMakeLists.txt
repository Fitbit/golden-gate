# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_COAP "Enable coap library" TRUE)
if(NOT GG_LIBS_ENABLE_COAP)
    return()
endif()

set(SOURCES gg_coap.c gg_coap_endpoint.c gg_coap_message.c gg_coap_blockwise.c gg_coap_filters.c)
set(HEADERS gg_coap.h gg_coap_endpoint.h gg_coap_message.h gg_coap_blockwise.h gg_coap_filters.h)

add_subdirectory(handlers)

add_library(gg-coap ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-coap)

target_link_libraries(gg-coap PRIVATE gg-annotations
                                      gg-common
                                      gg-sockets
                                      gg-loop)

set_target_properties(gg-coap PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-coap EXPORT golden-gate
                        ARCHIVE DESTINATION lib
                        PUBLIC_HEADER DESTINATION include/xp/coap)
