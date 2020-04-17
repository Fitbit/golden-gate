# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_COAP_EXAMPLE "Enable CoAP example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_COAP_EXAMPLE)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-coap-blockwise-example coap_blockwise_example.c)
    target_link_libraries(gg-coap-blockwise-example PRIVATE gg-runtime)

    add_executable(gg-coap-splitter-example coap_splitter_example.c)
    target_link_libraries(gg-coap-splitter-example PRIVATE gg-runtime)
endif()
