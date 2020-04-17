# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_PORTS_ENABLE_WINSOCK "Enable Winsock" FALSE)
if(NOT GG_PORTS_ENABLE_WINSOCK)
    return()
endif()

target_sources(gg-module PRIVATE ports/windows/gg_winsock.c)

# winsock needs an init function to be called
list(APPEND GG_MODULE_INIT_LIST WINSOCK)
set(GG_MODULE_INIT_LIST ${GG_MODULE_INIT_LIST} PARENT_SCOPE)

# for this module, we need to link in the winsock library
target_link_libraries(gg-module PUBLIC wsock32 ws2_32)
