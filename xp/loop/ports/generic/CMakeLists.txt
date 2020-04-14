option(GG_PORTS_ENABLE_GENERIC_LOOP "Enable Generic Loop" FALSE)
if(NOT GG_PORTS_ENABLE_GENERIC_LOOP)
    return()
endif()

target_sources(gg-loop PRIVATE ports/generic/gg_generic_loop.c)
