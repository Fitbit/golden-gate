# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Root for this port subdirectory
set(PORT_DIR ports/bsd)

option(GG_PORTS_ENABLE_BSD_SOCKETS "Enable BSD Sockets" FALSE)
if(NOT GG_PORTS_ENABLE_BSD_SOCKETS)
    return()
endif()

target_sources(gg-sockets PRIVATE ${PORT_DIR}/gg_bsd_sockets.c
                                  ${PORT_DIR}/gg_bsd_sockets.h)

