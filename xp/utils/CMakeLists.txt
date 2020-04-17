# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_UTILS "Enable utils library" TRUE)
if(NOT GG_LIBS_ENABLE_UTILS)
    return()
endif()

set(SOURCES gg_async_pipe.c
            gg_memory_data_source.c
            gg_memory_data_sink.c
            gg_blaster_data_source.c
            gg_perf_data_sink.c
            gg_print_data_sink.c
            gg_data_probe.c
            gg_activity_data_monitor.c
            gg_coap_event_emitter.c)
set(HEADERS gg_async_pipe.h
            gg_memory_data_source.h
            gg_memory_data_sink.h
            gg_blaster_data_source.h
            gg_perf_data_sink.h
            gg_print_data_sink.h
            gg_data_probe.h
            gg_activity_data_monitor.h
            gg_coap_event_emitter.h)

add_library(gg-utils ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-utils)

target_link_libraries(gg-utils PRIVATE gg-common)

set_target_properties(gg-utils PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-utils EXPORT golden-gate
                         ARCHIVE DESTINATION lib
                         PUBLIC_HEADER DESTINATION include/xp/utils)
