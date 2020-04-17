# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_PORTS_ENABLE_LWIP)
    return()
endif()

gg_add_test(test_gg_lwip.cpp "gg-runtime")
