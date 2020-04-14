option(GG_LIBS_ENABLE_COAP_HANDLERS "Enable CoAP handlers library" TRUE)
if(NOT GG_LIBS_ENABLE_COAP_HANDLERS)
    return()
endif()

set(SOURCES gg_coap_splitter.c gg_coap_helloworld_handler.c)
set(HEADERS gg_coap_splitter.h gg_coap_helloworld_handler.h)

add_library(gg-coap-handlers ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-coap-handlers)

target_link_libraries(gg-coap-handlers PRIVATE gg-coap
                                               gg-common)

set_target_properties(gg-coap-handlers PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-coap-handlers EXPORT golden-gate
                                 ARCHIVE DESTINATION lib
                                 PUBLIC_HEADER DESTINATION include/xp/coap/handlers)
