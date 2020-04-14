CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_GATTLINK_EXAMPLES "Enable gattlink examples" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_GATTLINK_EXAMPLES)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-gattlink-blast-over-udp-example gattlink_blast_over_udp.c)
    target_link_libraries(gg-gattlink-blast-over-udp-example PRIVATE gg-runtime)
endif()
