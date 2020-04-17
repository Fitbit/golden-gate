# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_PORTS_ENABLE_NUTTX_NETIF "Enable nuttx netif port" FALSE)
if(NOT GG_PORTS_ENABLE_NUTTX_NETIF)
    return()
endif()

set(SOURCES gg_nuttx_generic_netif.c)
set(HEADERS gg_nuttx_generic_netif.h)

add_library(gg-nuttx-netif ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-nuttx-netif)

target_link_libraries(gg-nuttx-netif PRIVATE gg-common)

set_target_properties(gg-nuttx-netif PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-nuttx-netif EXPORT golden-gate
                               ARCHIVE DESTINATION lib
                               PUBLIC_HEADER DESTINATION include/xp/netif/nuttx)
