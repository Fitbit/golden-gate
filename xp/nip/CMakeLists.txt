option(GG_PORTS_ENABLE_NIP "Enable NIP" FALSE)
if(NOT GG_PORTS_ENABLE_NIP)
    return()
endif()
option(GG_PORTS_ENABLE_NIP_NETIF "Enable NIP Network Interface" FALSE)

set(SOURCES gg_nip.c)
set(HEADERS gg_nip.h)

add_library(gg-nip ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-nip)

target_link_libraries(gg-nip PRIVATE gg-common gg-protocols)

# the NIP module needs an init function to be called
list(APPEND GG_MODULE_INIT_LIST NIP)
list(APPEND GG_MODULE_DEPS_LIST gg-nip)
set(GG_MODULE_INIT_LIST ${GG_MODULE_INIT_LIST} PARENT_SCOPE)
set(GG_MODULE_DEPS_LIST ${GG_MODULE_DEPS_LIST} PARENT_SCOPE)

set_target_properties(gg-nip PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-nip EXPORT golden-gate
                       ARCHIVE DESTINATION lib
                       PUBLIC_HEADER DESTINATION include/xp/nip)
