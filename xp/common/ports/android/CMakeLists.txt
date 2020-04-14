# Root for this port subdirectory
set(PORT_DIR ports/android)

option(GG_PORTS_ENABLE_ANDROID_LOG_CONFIG "Enable Android log config" FALSE)
if(GG_PORTS_ENABLE_ANDROID_LOG_CONFIG)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_android_log_config.c)

    # we need to link in the liblog.so library
    target_link_libraries(gg-common PUBLIC log)
endif()

option(GG_PORTS_ENABLE_ANDROID_CONSOLE "Enable Android console" FALSE)
if(GG_PORTS_ENABLE_ANDROID_CONSOLE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_android_console.c)

    # we need to link in the liblog.so library
    target_link_libraries(gg-common PUBLIC log)
endif()
