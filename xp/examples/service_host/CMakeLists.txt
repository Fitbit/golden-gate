CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_SERVICE_HOST_EXAMPLE "Enable service host example" ON "GG_ENABLE_EXAMPLES AND GG_LIBS_ENABLE_REMOTE" OFF)
if(NOT GG_APPS_ENABLE_SERVICE_HOST_EXAMPLE)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS AND GG_PORTS_ENABLE_POSIX_THREADS)
    add_executable(gg-service-host-example service_host_example.c)
    target_link_libraries(gg-service-host-example PRIVATE gg-runtime gg-coap-client-service gg-blast-service gg-stack-service)
endif()
