# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_COAP_TEST_SERVICE "Enable coap test service" TRUE)
if(NOT GG_LIBS_ENABLE_COAP_TEST_SERVICE)
    return()
endif()

set(SOURCES gg_coap_test_service.c gg_coap_test_service_private.c)
set(HEADERS gg_coap_test_service.h gg_coap_test_service_private.h)

add_library(gg-coap-test-service ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-coap-test-service)

target_link_libraries(gg-coap-test-service PRIVATE gg-common
                                                   gg-loop
                                                   gg-utils
                                           PUBLIC gg-remote)

set_target_properties(gg-coap-test-service PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-coap-test-service EXPORT golden-gate
                                     ARCHIVE DESTINATION lib
                                     PUBLIC_HEADER DESTINATION include/xp/remote)
