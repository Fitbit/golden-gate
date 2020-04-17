# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_EXAMPLE_INSPECT "Enable inspect example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_EXAMPLE_INSPECT)
    return()
endif()

add_executable(gg-inspect-example inspect_example.c)
target_link_libraries(gg-inspect-example PRIVATE gg-runtime)

