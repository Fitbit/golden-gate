# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_COAP_CLIENT "Enable Coap Client" ON "GG_ENABLE_EXAMPLES AND GG_CONFIG_ENABLE_REMOTE_API" OFF)
if(NOT GG_APPS_ENABLE_COAP_CLIENT)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-coap-client gg_coap_client.c)
    target_link_libraries(gg-coap-client PRIVATE gg-runtime)
endif()
