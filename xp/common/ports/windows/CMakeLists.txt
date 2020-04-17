# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Root for this port subdirectory
set(PORT_DIR ports/windows)

option(GG_PORTS_ENABLE_WINDOWS_TIME "Enable Windows time" FALSE)
if(GG_PORTS_ENABLE_WINDOWS_TIME)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_windows_time.c)
endif()

option(GG_PORTS_ENABLE_WINDOWS_THREADS "Enable Windows threads" FALSE)
if(GG_PORTS_ENABLE_WINDOWS_THREADS)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_windows_threads.c)
endif()

option(GG_PORTS_ENABLE_WINDOWS_SHARED_QUEUE "Enable Windows shared queue" FALSE)
if(GG_PORTS_ENABLE_WINDOWS_SHARED_QUEUE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_windows_shared_queue.c)
endif()
