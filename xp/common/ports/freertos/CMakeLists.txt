# Root for this port subdirectory
set(PORT_DIR ports/freertos)

option(GG_PORTS_ENABLE_FREERTOS_SHARED_QUEUE "Enable FreeRTOS Shared Queue support" FALSE)
if(GG_PORTS_ENABLE_FREERTOS_SHARED_QUEUE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_freertos_shared_queue.c)
    target_link_libraries(gg-common PRIVATE)
    target_include_directories(gg-common PRIVATE ${FREERTOS_ROOT}/include ${FREERTOS_EXTRA_INCLUDES})
endif()

option(GG_PORTS_ENABLE_FREERTOS_SHARED_QUEUE "Enable FreeRTOS Threads support" FALSE)
if(GG_PORTS_ENABLE_FREERTOS_THREADS)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_freertos_threads.c)
    target_link_libraries(gg-common PRIVATE)
    target_include_directories(gg-common PRIVATE ${FREERTOS_ROOT}/include ${FREERTOS_EXTRA_INCLUDES})
endif()

option(GG_PORTS_ENABLE_FREERTOS_TIME "Enable FreeRTOS Time support" FALSE)
if(GG_PORTS_ENABLE_FREERTOS_TIME)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_freertos_time.c)
    target_link_libraries(gg-common PRIVATE)
    target_include_directories(gg-common PRIVATE ${FREERTOS_ROOT}/include ${FREERTOS_EXTRA_INCLUDES})
endif()
