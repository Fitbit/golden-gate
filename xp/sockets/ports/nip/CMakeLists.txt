# Root for this port subdirectory
set(PORT_DIR ports/nip)

option(GG_PORTS_ENABLE_NIP_SOCKETS "Enable NIP Sockets" FALSE)
if(NOT GG_PORTS_ENABLE_NIP_SOCKETS)
    return()
endif()

target_sources(gg-sockets PRIVATE ${PORT_DIR}/gg_nip_sockets.c
                                  ${PORT_DIR}/gg_nip_sockets.h)

target_link_libraries(gg-sockets PRIVATE gg-nip)
