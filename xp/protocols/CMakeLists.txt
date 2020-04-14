option(GG_LIBS_ENABLE_PROTOCOLS "Enable protocols library" TRUE)
if(NOT GG_LIBS_ENABLE_PROTOCOLS)
    return()
endif()

set(SOURCES gg_protocols.c gg_ipv4_protocol.c)
set(HEADERS gg_protocols.h gg_ipv4_protocol.h)

add_library(gg-protocols ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-protocols)

target_link_libraries(gg-protocols PUBLIC gg-annotations gg-common)

set_target_properties(gg-protocols PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-protocols EXPORT golden-gate
                             ARCHIVE DESTINATION lib
                             PUBLIC_HEADER DESTINATION include/xp/protocols)
