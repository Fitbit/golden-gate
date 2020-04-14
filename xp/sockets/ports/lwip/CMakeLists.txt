# Root for this port subdirectory
set(PORT_DIR ports/lwip)

option(GG_PORTS_ENABLE_LWIP_SOCKETS "Enable LWIP Sockets" FALSE)
option(GG_PORTS_ENABLE_LWIP_NETCONN_SOCKETS "Enable LWIP Netconn Sockets" FALSE)
if(NOT GG_PORTS_ENABLE_LWIP_SOCKETS AND NOT GG_PORTS_ENABLE_LWIP_NETCONN_SOCKETS)
    return()
endif()
if (GG_PORTS_ENABLE_LWIP_SOCKETS AND GG_PORTS_ENABLE_LWIP_NETCONN_SOCKETS)
    message(FATAL_ERROR "GG_PORTS_ENABLE_LWIP_SOCKETS and GG_PORTS_ENABLE_LWIP_NETCONN_SOCKETS are mutually exclusive")
endif()

if (GG_PORTS_ENABLE_LWIP_SOCKETS)
    target_sources(gg-sockets PRIVATE ${PORT_DIR}/gg_lwip_sockets.c
                                      ${PORT_DIR}/gg_lwip_sockets.h)
    # silence some warnings, because of LwIP headers that are not 100% clean
    set_source_files_properties(${PORT_DIR}/gg_lwip_sockets.c PROPERTIES COMPILE_FLAGS -Wno-conversion)
else()
    target_sources(gg-sockets PRIVATE ${PORT_DIR}/gg_lwip_netconn_sockets.c
                                      ${PORT_DIR}/gg_lwip_sockets.h)
endif()

target_link_libraries(gg-sockets PRIVATE gg-annotations gg-loop)
if(GG_PORTS_ENABLE_LWIP)
    target_link_libraries(gg-sockets PRIVATE gg-lwip)
else()
    target_include_directories(gg-sockets PRIVATE ${LWIP_INCLUDES})
endif()
