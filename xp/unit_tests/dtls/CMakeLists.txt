# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_LIBS_ENABLE_TLS)
    return()
endif()

gg_add_test(test_gg_dtls.cpp "gg-common;gg-tls;gg-runtime")
