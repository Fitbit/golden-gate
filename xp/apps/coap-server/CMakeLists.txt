CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_COAP_SERVER "Enable Coap Server" ON "GG_ENABLE_APPS AND GG_LIBS_ENABLE_REMOTE" OFF)
if(NOT GG_APPS_ENABLE_COAP_SERVER)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-coap-server gg_coap_server.c)
    target_link_libraries(gg-coap-server PRIVATE gg-runtime
                                                gg-coap-test-service)
endif()
