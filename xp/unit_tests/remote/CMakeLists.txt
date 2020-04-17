# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_LIBS_ENABLE_REMOTE)
    return()
endif()

gg_add_test(test_gg_remote_parser.cpp gg-remote)
