# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_REMOTE_EXAMPLE "Enable remote examples" ON "GG_ENABLE_EXAMPLES AND GG_LIBS_ENABLE_REMOTE" OFF)
if(NOT GG_APPS_ENABLE_REMOTE_EXAMPLE)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-remote-shell-over-udp-example remote_shell_over_udp_example.c)
    target_link_libraries(gg-remote-shell-over-udp-example PRIVATE gg-runtime)
endif()
