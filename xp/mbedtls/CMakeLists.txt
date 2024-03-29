# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_PORTS_ENABLE_MBEDTLS "Enable building the MbedTLS library" TRUE)
if(NOT GG_PORTS_ENABLE_MBEDTLS)
    return()
endif()

# We can build with MbedTLS version 2.x or 3.x
set(GG_CONFIG_MBEDTLS_VERSION "2" CACHE STRING "MbedTLS version (2 or 3)")

# Root of the MbedTLS distribution
if(GG_CONFIG_MBEDTLS_VERSION STREQUAL "3")
    set(MBEDTLS_ROOT ${GG_ROOT}/external/mbedtls-3)
    set(MBEDTLS_DEFAULT_CONFIG "gg_dtls_default_config_v3.h")
elseif(GG_CONFIG_MBEDTLS_VERSION STREQUAL "2" )
    set(MBEDTLS_ROOT ${GG_ROOT}/external/mbedtls)
    set(MBEDTLS_DEFAULT_CONFIG "gg_dtls_default_config.h")
else()
    message(FATAL_ERROR "Invalid value for GG_MBEDTLS_VERSION")
    error(xxx)
endif()

# Default value for the config header file (can be changed in platform config CMake initial cache files)
set(GG_MBEDTLS_CONFIG ${MBEDTLS_DEFAULT_CONFIG} CACHE STRING "MbedTLS configuration header file name")

file(GLOB MBEDTLS_SRC_FILES ${MBEDTLS_ROOT}/library/*.c)
list(REMOVE_ITEM MBEDTLS_SRC_FILES ${MBEDTLS_ROOT}/library/net_sockets.c)

# If the build profile requires a custom allocator, we need an init function
option(GG_CONFIG_MBEDTLS_USE_CUSTOM_ALLOCATOR "Use a custom allocator for mbedtls" FALSE)
if(GG_CONFIG_MBEDTLS_USE_CUSTOM_ALLOCATOR)
    list(APPEND GG_MODULE_INIT_LIST MBEDTLS)
    list(APPEND MBEDTLS_SRC_FILES gg_mbedtls.c)
    set(GG_MODULE_INIT_LIST ${GG_MODULE_INIT_LIST} PARENT_SCOPE)
endif()

add_library(gg-mbedtls ${MBEDTLS_SRC_FILES})

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(gg-mbedtls PRIVATE -Wno-reserved-id-macro -Wno-switch-enum -Wno-cast-align -Wno-cast-qual -Wno-conversion -Wno-overlength-strings -Wno-covered-switch-default -Wno-format-nonliteral -Wno-unused-macros -Wno-conditional-uninitialized)
endif()
target_include_directories(gg-mbedtls PUBLIC ${MBEDTLS_ROOT}/include configs)
target_compile_definitions(gg-mbedtls PRIVATE MBEDTLS_CONFIG_FILE=\"${GG_MBEDTLS_CONFIG}\")
