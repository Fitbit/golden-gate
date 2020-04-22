# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Those two options are independent, because on some platforms, we may need
# to compile the LwIP netif, but not the LwIP library, when that library is
# already part of the platform.
option(GG_PORTS_ENABLE_LWIP "Enable LwIP port" FALSE)
if (GG_PORTS_ENABLE_LWIP)
    option(GG_PORTS_ENABLE_LWIP_NETIF "Enable LwIP netif port" TRUE)
else()
    option(GG_PORTS_ENABLE_LWIP_NETIF "Enable LwIP netif port" FALSE)
endif()

if (GG_PORTS_ENABLE_LWIP)
    # the LWIP module needs an init function to be called
    list(APPEND GG_MODULE_INIT_LIST LWIP)
    list(APPEND GG_MODULE_DEPS_LIST gg-lwip)
    set(GG_MODULE_INIT_LIST ${GG_MODULE_INIT_LIST} PARENT_SCOPE)
    set(GG_MODULE_DEPS_LIST ${GG_MODULE_DEPS_LIST} PARENT_SCOPE)

    set(LWIP_ROOT ${GG_ROOT}/external/lwip)

    option(LWIP_NETIF_LOOPBACK "Enable loopback interface" FALSE)
    option(LWIP_HAVE_LOOPIF "Register loopback interface" FALSE)
    option(LWIP_DEBUG "Enable lwIP debug logging" FALSE)

    # on some platforms, we may need to rename some of LWIP's functions so as to not
    # conflict with existing symbols already present on the platform
    set(GG_CONFIG_LWIP_REMAP_SYMBOLS "" CACHE STRING "LWIP symbol remapping list")
    if (GG_CONFIG_LWIP_REMAP_SYMBOLS)
        foreach(SYMBOL IN LISTS GG_CONFIG_LWIP_REMAP_SYMBOLS)
            set (LWIP_RENAME_DEFINES "${LWIP_RENAME_DEFINES} #define ${SYMBOL} lwip_${SYMBOL}\n")
        endforeach()
    endif()

    set(lwipopts_header_dir "${PROJECT_BINARY_DIR}/config")
    set(lwipopts_header "${lwipopts_header_dir}/lwipopts.h")
    configure_file(lwipopts.h.in "${lwipopts_header}")

    file(GLOB LWIP_SRC_FILES gg_lwip.c
                            ${LWIP_ROOT}/src/core/*.c
                            ${LWIP_ROOT}/src/core/ipv4/*.c)

    set(GG_CONFIG_LWIP_PORT "generic" CACHE STRING "LWIP system port")
    set(LWIP_PORT_FILE ${GG_ROOT}/xp/lwip/ports/${GG_CONFIG_LWIP_PORT}/sys_arch.c)
    set(LWIP_PORT_INCLUDE_DIR ports/${GG_CONFIG_LWIP_PORT})

    add_library(gg-lwip ${LWIP_SRC_FILES}
                        ${LWIP_PORT_FILE})

    if (GG_CONFIG_ENABLE_TCP)
        target_compile_definitions(gg-lwip PRIVATE GG_CONFIG_ENABLE_TCP)
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(gg-lwip PRIVATE -Wno-conversion -Wno-format-nonliteral -Wno-newline-eof -Wno-missing-variable-declarations -Wno-unused-macros -Wno-packed -Wno-covered-switch-default -Wno-gnu-empty-initializer -Wno-zero-length-array -Wno-unused-variable -Wno-type-limits -Wno-unused-parameter)
    endif()
    target_include_directories(gg-lwip PUBLIC ${LWIP_ROOT}/src/include
                                              ${PROJECT_BINARY_DIR}/config
                                              ${LWIP_PORT_INCLUDE_DIR})
    target_link_libraries(gg-lwip PRIVATE gg-common)
    gg_add_to_all_libs(gg-lwip)
endif()

if (GG_PORTS_ENABLE_LWIP_NETIF)
    add_library(gg-lwip-netif gg_lwip_generic_netif.c gg_lwip_generic_netif.h)
    if (GG_PORTS_ENABLE_LWIP)
        target_include_directories(gg-lwip-netif PUBLIC ${LWIP_ROOT}/src/include
                                                        ${PROJECT_BINARY_DIR}/config
                                                        ${LWIP_PORT_INCLUDE_DIR})
    endif()
    target_link_libraries(gg-lwip-netif PRIVATE gg-common)
    list(APPEND GG_MODULE_DEPS_LIST gg-lwip-netif)
    gg_add_to_all_libs(gg-lwip-netif)
endif()
