# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Root for this port subdirectory
set(PORT_DIR ports/apple)

option(GG_PORTS_ENABLE_APPLE_TIME "Enable Apple Time support" FALSE)
if(GG_PORTS_ENABLE_APPLE_TIME)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_apple_time.c)
endif()

option(GG_PORTS_ENABLE_APPLE_SEMAPHORE "Enable Apple Semaphore support" FALSE)
if(GG_PORTS_ENABLE_APPLE_SEMAPHORE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_apple_semaphore.c)
endif()

option(GG_PORTS_ENABLE_APPLE_AUTORELEASE "Enable Apple Autorelease support" FALSE)
if(GG_PORTS_ENABLE_APPLE_AUTORELEASE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_apple_autorelease.m)
endif()

# Custom Pre build phase placeholder updated by gg_post_process_xcode_project
# to generate xp/common/gg_version_info.h at build time
if (CMAKE_GENERATOR MATCHES "Xcode")
    add_custom_command(TARGET gg-common PRE_BUILD
        COMMAND true
    )
endif()
