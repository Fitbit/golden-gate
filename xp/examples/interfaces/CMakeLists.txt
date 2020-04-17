# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_INTERFACES_EXAMPLE "Enable interfaces example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_INTERFACES_EXAMPLE)
    return()
endif()

add_executable(gg-interfaces-example gg_interfaces_example.c)
target_link_libraries(gg-interfaces-example PRIVATE gg-runtime)

add_executable(gg-interfaces-trap-example gg_interfaces_trap_example.c)
target_link_libraries(gg-interfaces-trap-example PRIVATE gg-runtime)
