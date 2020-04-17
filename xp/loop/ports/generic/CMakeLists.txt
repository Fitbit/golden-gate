# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_PORTS_ENABLE_GENERIC_LOOP "Enable Generic Loop" FALSE)
if(NOT GG_PORTS_ENABLE_GENERIC_LOOP)
    return()
endif()

target_sources(gg-loop PRIVATE ports/generic/gg_generic_loop.c)
