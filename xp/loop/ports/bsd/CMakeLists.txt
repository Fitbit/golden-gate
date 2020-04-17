# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_PORTS_ENABLE_BSD_SELECT_LOOP "Enable BSD Select Loop" FALSE)
if(NOT GG_PORTS_ENABLE_BSD_SELECT_LOOP)
    return()
endif()

option(GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION "Enable BSD socketpair emulation" FALSE)
if(GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION)
    target_compile_definitions(gg-loop PUBLIC GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION)
endif()

option(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD "Enable per-pid socketpair FD" FALSE)
if(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
    target_compile_definitions(gg-loop PUBLIC GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
endif()

target_sources(gg-loop PRIVATE ports/bsd/gg_bsd_select_loop.c
                               extensions/gg_loop_fd.h)
