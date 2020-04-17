# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Root for this port subdirectory
set(PORT_DIR ports/posix)

option(GG_PORTS_ENABLE_POSIX_TIME "Enable Posix Time support" FALSE)
if(GG_PORTS_ENABLE_POSIX_TIME)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_posix_time.c)
endif()

option(GG_PORTS_ENABLE_POSIX_THREADS "Enable Posix Thread support" FALSE)
if(GG_PORTS_ENABLE_POSIX_THREADS)
    find_package( Threads )
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_posix_threads.c)
    target_link_libraries(gg-common PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

option(GG_PORTS_ENABLE_POSIX_MUTEX "Enable Posix Mutex support" FALSE)
if(GG_PORTS_ENABLE_POSIX_MUTEX)
    find_package( Threads )
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_posix_mutex.c)
    target_link_libraries(gg-common PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

option(GG_PORTS_ENABLE_POSIX_SEMAPHORE "Enable Posix Semaphore support" FALSE)
if(GG_PORTS_ENABLE_POSIX_SEMAPHORE)
    find_package( Threads )
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_posix_semaphore.c)
    target_link_libraries(gg-common PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

option(GG_PORTS_ENABLE_POSIX_SHARED_QUEUE "Enable Posix Shared Queue support" FALSE)
if(GG_PORTS_ENABLE_POSIX_SHARED_QUEUE)
    find_package( Threads )
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_posix_shared_queue.c)
    target_link_libraries(gg-common PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()
