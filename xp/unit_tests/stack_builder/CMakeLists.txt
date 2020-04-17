# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

if(NOT GG_LIBS_ENABLE_STACK_BUILDER)
    return()
endif()

gg_add_test(test_gg_stack_builder.cpp "gg-stack-builder;gg-loop;gg-utils")
