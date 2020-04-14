CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_SOCKETS_EXAMPLE "Enable sockets example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_SOCKETS_EXAMPLE)
    return()
endif()

add_executable(gg-udp-echo-server-example udp_echo_server.c)
target_link_libraries(gg-udp-echo-server-example PRIVATE gg-runtime)

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-udp-blast-example udp_blast.c)
    target_link_libraries(gg-udp-blast-example PRIVATE gg-runtime)
endif()

if (GG_PORTS_ENABLE_LWIP_SOCKETS AND GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-lwip-sockets-example lwip_sockets.c)
    target_link_libraries(gg-lwip-sockets-example PRIVATE gg-runtime)

    add_executable(gg-lwip-over-bsd-blast-example udp_lwip_over_bsd_blast.c)
    target_link_libraries(gg-lwip-over-bsd-blast-example PRIVATE gg-runtime)
endif()

if (GG_PORTS_ENABLE_NIP_SOCKETS AND GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-nip-over-bsd-blast-example udp_nip_over_bsd_blast.c)
    target_link_libraries(gg-nip-over-bsd-blast-example PRIVATE gg-runtime)
endif()
