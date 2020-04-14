option(GG_LIBS_ENABLE_TLS "Enable TLS library" TRUE)
if(NOT GG_LIBS_ENABLE_TLS)
    return()
endif()

set(SOURCES gg_tls.c)
set(HEADERS gg_tls.h)

add_library(gg-tls ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-tls)

target_link_libraries(gg-tls PRIVATE gg-common)

include(ports/mbedtls/CMakeLists.txt)

set_target_properties(gg-tls PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-tls EXPORT golden-gate
                       ARCHIVE DESTINATION lib
                       PUBLIC_HEADER DESTINATION include/xp/tls)
