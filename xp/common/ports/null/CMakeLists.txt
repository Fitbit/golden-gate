# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Root for this port subdirectory
set(PORT_DIR ports/null)

option(GG_PORTS_ENABLE_NULL_LOG_CONFIG "Enable Null Log Config" FALSE)
if(GG_PORTS_ENABLE_NULL_LOG_CONFIG)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_null_log_config.c)
endif()

option(GG_PORTS_ENABLE_NULL_ENVIRONMENT "Enable Null Environment support" FALSE)
if(GG_PORTS_ENABLE_NULL_ENVIRONMENT)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_null_environment.c)
endif()

option(GG_PORTS_ENABLE_NULL_THREADS "Enable Null Threads APIs" FALSE)
if(GG_PORTS_ENABLE_NULL_THREADS)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_null_threads.c)
endif()

option(GG_PORTS_ENABLE_NULL_AUTORELEASE "Enable Null Autorelease suport" TRUE)
if(GG_PORTS_ENABLE_NULL_AUTORELEASE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_null_autorelease.c)
endif()
