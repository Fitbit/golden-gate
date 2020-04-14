option(GG_LIBS_ENABLE_SOCKETS "Enable sockets library" TRUE)
if(NOT GG_LIBS_ENABLE_SOCKETS)
    return()
endif()

add_library(gg-sockets gg_sockets.c gg_sockets.h)
gg_add_to_all_libs(gg-sockets)

target_link_libraries(gg-sockets PRIVATE gg-common)

include(ports/bsd/CMakeLists.txt)
include(ports/lwip/CMakeLists.txt)
include(ports/nip/CMakeLists.txt)

if (GG_CONFIG_DEFAULT_SOCKETS_MODULE)
    target_compile_definitions(gg-sockets PRIVATE GG_CONFIG_DEFAULT_SOCKETS_MODULE=${GG_CONFIG_DEFAULT_SOCKETS_MODULE})
endif()

set_target_properties(gg-sockets PROPERTIES PUBLIC_HEADER gg_sockets.h)

install(TARGETS gg-sockets EXPORT golden-gate
                           ARCHIVE DESTINATION lib
                           PUBLIC_HEADER DESTINATION include/xp/sockets)
