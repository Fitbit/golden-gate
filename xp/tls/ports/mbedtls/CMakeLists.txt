# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_CONFIG_MBEDTLS_USE_PLATFORM_RNG "Use platform-provided mbedtls RNG init function" FALSE)
option(GG_PORTS_ENABLE_MBEDTLS_TLS "Enable mbedtls TLS" FALSE)
if(NOT GG_PORTS_ENABLE_MBEDTLS_TLS)
    return()
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(gg-tls PRIVATE -Wno-switch-enum)
endif()

if(GG_CONFIG_MBEDTLS_USE_PLATFORM_RNG)
    target_compile_definitions(gg-tls PRIVATE GG_CONFIG_MBEDTLS_USE_PLATFORM_RNG)
endif()

if(GG_CONFIG_MBEDTLS_USE_PLATFORM_SETUP)
    target_compile_definitions(gg-tls PRIVATE GG_CONFIG_MBEDTLS_USE_PLATFORM_SETUP)
endif()

target_sources(gg-tls PRIVATE ports/mbedtls/gg_mbedtls_tls.c
                              ports/mbedtls/gg_mbedtls_tls.h)

target_link_libraries(gg-tls PRIVATE gg-annotations gg-common)

# If we use a local build of mbedtls
if (GG_PORTS_ENABLE_MBEDTLS)
    # set the default value for the config header file (can be changed in platform config CMake initial cache files)
    set(GG_MBEDTLS_CONFIG "gg_dtls_default_config.h" CACHE STRING "")
    target_compile_definitions(gg-tls PRIVATE MBEDTLS_CONFIG_FILE=\"${GG_MBEDTLS_CONFIG}\")

    # add a dependency to the local mbedtls library
    target_link_libraries(gg-tls PRIVATE gg-mbedtls)
endif()
