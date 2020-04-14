option(GG_LIBS_ENABLE_STACK_BUILDER "Enable stack builder" TRUE)
if(NOT GG_LIBS_ENABLE_STACK_BUILDER)
    return()
endif()

set(SOURCES gg_stack_builder.c)
set(HEADERS gg_stack_builder.h)

add_library(gg-stack-builder ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-stack-builder)

target_link_libraries(gg-stack-builder PRIVATE gg-common gg-sockets gg-tls gg-gattlink gg-protocols)
if (GG_PORTS_ENABLE_LWIP_NETIF)
    target_sources(gg-stack-builder PRIVATE ports/lwip/gg_lwip_stack_builder.c)
    target_link_libraries(gg-stack-builder PRIVATE gg-lwip-netif)
endif()
if (GG_PORTS_ENABLE_NUTTX_NETIF)
    target_sources(gg-stack-builder PRIVATE ports/nuttx/gg_nuttx_stack_builder.c)
    target_link_libraries(gg-stack-builder PRIVATE gg-nuttx-netif)
endif()
if (GG_PORTS_ENABLE_NIP_NETIF)
    target_sources(gg-stack-builder PRIVATE ports/nip/gg_nip_stack_builder.c)
    target_link_libraries(gg-stack-builder PRIVATE gg-nip)
endif()

set_target_properties(gg-stack-builder PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-stack-builder EXPORT golden-gate
                                 ARCHIVE DESTINATION lib
                                 PUBLIC_HEADER DESTINATION include/xp/stack-builder)
