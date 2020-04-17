# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_PORTS_ENABLE_NIP)
    return()
endif()

gg_add_test(test_gg_nip.cpp "gg-common;gg-module;gg-sockets;gg-utils;gg-loop")
