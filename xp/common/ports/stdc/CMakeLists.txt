# Root for this port subdirectory
set(PORT_DIR ports/stdc)

option(GG_PORTS_ENABLE_STDC_ENV "Enable Standard C environment" FALSE)
if(GG_PORTS_ENABLE_STDC_ENV)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_stdc_environment.c)
endif()

option(GG_PORTS_ENABLE_STDC_CONSOLE "Enable Standard C console" FALSE)
if(GG_PORTS_ENABLE_STDC_CONSOLE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_stdc_console.c)
endif()

option(GG_PORTS_ENABLE_STDC_RANDOM "Enable Standard C random" FALSE)
if(GG_PORTS_ENABLE_STDC_RANDOM)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_stdc_random.c)
endif()

option(GG_PORTS_ENABLE_STDC_MEMORY "Enable Standard C memory" TRUE)
if(GG_PORTS_ENABLE_STDC_MEMORY)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_stdc_memory.c)
endif()
