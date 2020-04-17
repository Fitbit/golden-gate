# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_DTLS_EXAMPLE "Enable DTLS example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_DTLS_EXAMPLE)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-dtls-example dtls_example.c)
    target_link_libraries(gg-dtls-example PRIVATE gg-runtime)
endif()
