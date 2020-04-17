# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_COAP_CLIENT_SERVICE "Enable coap client service" TRUE)
if(NOT GG_LIBS_ENABLE_COAP_CLIENT_SERVICE)
    return()
endif()

set(SOURCES gg_coap_client_service.c)
set(HEADERS gg_coap_client_service.h)

add_library(gg-coap-client-service ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-coap-client-service)

target_link_libraries(gg-coap-client-service PRIVATE gg-common
                                                     gg-loop
                                                     gg-utils
                                                     gg-coap
                                             PUBLIC  gg-remote)

set_target_properties(gg-coap-client-service PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-coap-client-service EXPORT golden-gate
                                       ARCHIVE DESTINATION lib
                                       PUBLIC_HEADER DESTINATION include/xp/services/coap_client)
