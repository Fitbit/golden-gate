# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

gg_add_test(test_gg_sockets.cpp "gg-sockets;gg-runtime")

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    gg_add_test(test_gg_bsd_sockets.cpp "gg-sockets;gg-runtime")
endif()
