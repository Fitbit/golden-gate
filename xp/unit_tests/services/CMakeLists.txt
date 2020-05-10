# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_LIBS_ENABLE_REMOTE)
    return()
endif()

gg_add_test(test_gg_blast_service.cpp "gg-blast-service;gg-utils;gg-runtime")
gg_add_test(test_gg_stack_service.cpp "gg-stack-service;gg-utils;gg-runtime")
